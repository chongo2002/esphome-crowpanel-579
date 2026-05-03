#include "crowpanel_579.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/application.h"

namespace esphome {
namespace crowpanel_579 {

static const char *const TAG = "crowpanel_579";

// Each chip handles half: 50 bytes * 272 rows = 13600 bytes
static const uint32_t HALF_BUF = 13600;
// Full visible buffer: 99 bytes * 272 rows = 26928 bytes
static const uint32_t FULL_BUF = 26928;
// Bytes per row for each chip (50 bytes = 400 pixels)
static const uint32_t BYTES_PER_ROW = 99;
static const uint32_t BYTES_PER_HALF_ROW = 50;

size_t CrowPanel579::get_buffer_length_() {
  return FULL_BUF;
}

void CrowPanel579::setup() {                                                                                                         
  if (this->power_pin_ != nullptr) {
    this->power_pin_->setup();                                                                                                       
    this->power_pin_->digital_write(true);                                                                                           
    delay(100);                           
  }                                                                                                                                  
  this->dc_pin_->setup();                                                                                                            
  this->busy_pin_->setup();
  this->reset_pin_->setup();                                                                                                         
  this->spi_setup();        
                      
  this->init_internal_(this->get_buffer_length_());                                                                                  
  memset(this->buffer_, 0xFF, this->get_buffer_length_());  // init to white paper
  this->init_display_();                                                                                                             
  ESP_LOGI(TAG, "Setup complete, buffer=%d bytes", this->get_buffer_length_());                                                      
}

void CrowPanel579::update() {
  ESP_LOGI(TAG, "update: this=%p  buffer_=%p", (void*)this, (void*)this->buffer_);   
  // force-write a known value BEFORE do_update_                                                                                     
  this->buffer_[0] = 0xAB;
  int pre = 0;                                                                                                                       
  for (int i = 0; i < (int)FULL_BUF; i++) if (buffer_[i] != 0xFF) pre++;
  ESP_LOGI(TAG, "update pre do_update_: non_white=%d buf[0]=%02X", pre, buffer_[0]);                                                 
  ESP_LOGI(TAG, "UPD this=%p buf=%p", (void*)this, (void*)this->buffer_);                                                                                                                                        
  this->do_update_();
                                                                                                                                       
  int post = 0; 
  for (int i = 0; i < (int)FULL_BUF; i++) if (buffer_[i] != 0xFF) post++;
  ESP_LOGI(TAG, "update post do_update_: non_white=%d buf[0]=%02X", post, buffer_[0]);                                               
                                                                                                                                       
}  
                                                                                                                                       
// void CrowPanel579::update() {
//   this->do_update_();                                                                                                                
//   this->display();
// }

// void CrowPanel579::setup() {
//   if (this->power_pin_ != nullptr) {
//     this->power_pin_->setup();
//     this->power_pin_->digital_write(true);
//     delay(100);
//   }
//   this->dc_pin_->setup();
//   this->busy_pin_->setup();
//   this->reset_pin_->setup();
//   this->spi_setup();

//   // Allocate framebuffer
//   this->init_internal_(this->get_buffer_length_());  
//   this->init_display_();
//   ESP_LOGI(TAG, "Setup complete, buffer=%d bytes", this->get_buffer_length_());
// }

// void CrowPanel579::update() {                                                                                                        
//   this->do_update_();                                                                                                                
//   this->display();
// }

void CrowPanel579::display() {  
  int non_white = 0;
  for (int i = 0; i < (int)FULL_BUF; i++)
    if (this->buffer_[i] != 0xFF) non_white++;                                                                                       
  ESP_LOGI(TAG, "display(): non-white bytes=%d / %d", non_white, (int)FULL_BUF);                                                     
  // ... rest of display()      
  ESP_LOGD(TAG, "Display start, buf[49]=%02X buf[50]=%02X",
          this->buffer_[49], this->buffer_[50]);                                                                                    
  
    // Slave: LEFT half — bytes 0-49, normal order (unchanged)                                                                         
  set_ram_slave_();
  send_command_(0xA4);
  this->dc_pin_->digital_write(true);
  this->enable();                                                                                                                    
  for (uint32_t y = 0; y < 272; y++) {
    uint32_t row_start = y * 99;                                                                                                     
    for (uint32_t b = 0; b < 50; b++) {
      this->write_byte(this->buffer_[row_start + b]);                                                                                
    }
    if (y % 68 == 0) App.feed_wdt();                                                                                                 
  }             
  this->disable();

    // Master: RIGHT half — bytes 49-98, NORMAL order, no pad                                                                          
    // X decrements 49→0: byte[49]→X=49=col392, byte[98]→X=0=col784
    // Was reversed (wrong): assumed X=0=leftmost, but X=49=leftmost on this chip                                                      
  set_ram_master_();                                                                                                                 
  send_command_(0x24);                                                                                                               
  this->dc_pin_->digital_write(true);                                                                                                
  this->enable();
  for (uint32_t y = 0; y < 272; y++) {
    uint32_t row_start = y * 99;                                                                                                     
    for (uint32_t b = 49; b <= 98; b++) {
      this->write_byte(this->buffer_[row_start + b]);                                                                                
    }                                                                                                                                
    if (y % 68 == 0) App.feed_wdt();
  }                                                                                                                                  
  this->disable();

  send_command_(0x22);
  send_data_(0xF7);
  send_command_(0x20);
  this->wait_busy_();                                                                                                                
}



void CrowPanel579::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x < 0 || x >= 792 || y < 0 || y >= 272)
    return;
  uint32_t pos = (y * 792 + x) / 8;
  uint8_t bit = 7 - (x % 8);
  if (color.is_on()) {                                                                                                               
    this->buffer_[pos] &= ~(1 << bit);   // on = black ink = 0
  } else {                                                                                                                           
    this->buffer_[pos] |= (1 << bit);    // off = white paper = 1                                                                    
  }                                                              
}    

void CrowPanel579::reset_() {
  delay(10);
  this->reset_pin_->digital_write(false);
  delay(10);
  this->reset_pin_->digital_write(true);
  delay(10);
  this->wait_busy_();
}

void CrowPanel579::wait_busy_() {
  uint32_t start = millis();
  while (this->busy_pin_->digital_read()) {
    if (millis() - start > 10000) {
      ESP_LOGE(TAG, "Timeout");
      break;
    }
    App.feed_wdt();
    delay(10);
  }
}

void CrowPanel579::send_command_(uint8_t cmd) {
  this->dc_pin_->digital_write(false);
  this->enable();
  this->write_byte(cmd);
  this->disable();
}

void CrowPanel579::send_data_(uint8_t data) {
  this->dc_pin_->digital_write(true);
  this->enable();
  this->write_byte(data);
  this->disable();
}

void CrowPanel579::draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr,                                        
                                  display::ColorOrder order, display::ColorBitness bitness,                                          
                                  bool big_endian, int x_offset, int y_offset, int x_pad) {                                          
  ESP_LOGI(TAG, "DPA this=%p buf=%p", (void*)this, (void*)this->buffer_);                                                            
                                                                                                                                       
  // --- pixel write loop ---                                                                                                        
  const uint8_t *src = ptr;                                                                                                          
  for (int y = y_start; y < y_start + h; y++) {                                                                                      
    for (int x = x_start; x < x_start + w; x++) {                                                                                    
      uint16_t pix = big_endian ? ((uint16_t)src[0]<<8|src[1]) : ((uint16_t)src[1]<<8|src[0]);                                       
      src += 2;                                                                                                                      
      if (x < 0 || x >= 792 || y < 0 || y >= 272) continue;                                                                          
      uint8_t r = (pix >> 11) << 3;                                                                                                  
      uint8_t g = ((pix >> 5) & 0x3F) << 2;                                                                                          
      uint8_t b = (pix & 0x1F) << 3;                                                                                                 
      uint32_t pos = (uint32_t)(y * 792 + x) / 8;                                                                                    
      uint8_t  bit = 7 - (x % 8);                                                                                                    
      if ((int)r + g + b >= 382) {                                                                                                   
        this->buffer_[pos] |= (1 << bit);   // light → white paper                                                                   
      } else {                                                                                                                       
        this->buffer_[pos] &= ~(1 << bit);  // dark → black ink                                                                      
      }                                                                                                                              
    }                                                                                                                                
    src += x_pad * 2;                                                                                                                
  }                                                                                                                                  
                                                                                                                                       
  // --- END diagnostic ---                                                                                                          
  int nw = 0;                                                                                                                        
  for (int i = 0; i < (int)FULL_BUF; i++) if (buffer_[i] != 0xFF) nw++;                                                              
  ESP_LOGI(TAG, "DPA END: non_white=%d buf[0]=%02X", nw, buffer_[0]);                                                                
} 
// void CrowPanel579::draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr,
//                                   display::ColorOrder order, display::ColorBitness bitness,                                          
//                                   bool big_endian, int x_offset, int y_offset, int x_pad) {
//   const uint8_t *src = ptr;                                                                                                          
//   for (int y = y_start; y < y_start + h; y++) {                                                                                    
//     for (int x = x_start; x < x_start + w; x++) {                                                                                    
//       uint16_t pix = big_endian                                                                                                      
//                   ? ((uint16_t)src[0] << 8 | src[1])
//                   : ((uint16_t)src[1] << 8 | src[0]);                                                                                
//       src += 2;                                                                                                                      
//       if (x < 0 || x >= 792 || y < 0 || y >= 272) continue;
//       uint8_t r = (pix >> 11) << 3;                                                                                                  
//       uint8_t g = ((pix >> 5) & 0x3F) << 2;                                                                                          
//       uint8_t b = (pix & 0x1F) << 3;
//       uint32_t pos = (uint32_t)(y * 792 + x) / 8;                                                                                    
//       uint8_t  bit = 7 - (x % 8);                                                                                                    
//       // Normal convention: light → white paper (1), dark → black ink (0)
//       if ((int)r + g + b >= 382) {                                                                                                   
//         this->buffer_[pos] |= (1 << bit);   // white paper
//       } else {                                                                                                                       
//         this->buffer_[pos] &= ~(1 << bit);  // black ink
//       }                                                                                                                              
//     }
//     src += x_pad * 2;                                                                                                                
//   }             
// }


void CrowPanel579::set_ram_master_() {
  send_command_(0x11);
  send_data_(0x02);
  send_command_(0x44);
  send_data_(0x31);
  send_data_(0x00);
  send_command_(0x45);
  send_data_(0x00);
  send_data_(0x00);
  send_data_(0x0F);
  send_data_(0x01);
  send_command_(0x4E);
  send_data_(0x31);
  send_command_(0x4F);
  send_data_(0x00);
  send_data_(0x00);
}

void CrowPanel579::set_ram_slave_() {
  send_command_(0x91);
  send_data_(0x03);
  send_command_(0xC4);
  send_data_(0x00);
  send_data_(0x31);
  send_command_(0xC5);
  send_data_(0x00);
  send_data_(0x00);
  send_data_(0x0F);
  send_data_(0x01);
  send_command_(0xCE);
  send_data_(0x00);
  send_command_(0xCF);
  send_data_(0x00);
  send_data_(0x00);
}

void CrowPanel579::write_ram_(uint8_t cmd, uint8_t fill, uint32_t count) {
  send_command_(cmd);
  this->dc_pin_->digital_write(true);
  this->enable();
  for (uint32_t i = 0; i < count; i++) {
    this->write_byte(fill);
    if (i % 1000 == 0) App.feed_wdt();
  }
  this->disable();
}

void CrowPanel579::init_display_() {
  this->reset_();                                                                                                                    
  this->wait_busy_();
  send_command_(0x12);   // SW reset
  this->wait_busy_();
  send_command_(0x18);                                                                                                             
  send_data_(0x80);      // use internal temperature sensor                                                                          
  send_command_(0x22);          
  send_data_(0xB1);      // enable clock, CP, load temp                                                                              
  send_command_(0x20);                                                                                                               
  this->wait_busy_();                                                                                                                
  send_command_(0x1A);                                                                                                               
  send_data_(0x64);                                                                                                                  
  send_data_(0x00);                                                                                                                  
  send_command_(0x22);                                                                                                             
  send_data_(0x91);      // enable clock, load LUT from OTP                                                                          
  send_command_(0x20);          
  this->wait_busy_();                                                                                                                
  send_command_(0x3C);                                                                                                               
  send_data_(0x01);      // border waveform                                                                                        
                                                                                                                                       
    // Clear both RAM banks to white on both chips.
    // Old RAM must match New RAM or the anti-ghosting waveform produces static.                                                       
  set_ram_master_();                                                                                                                 
  write_ram_(0x24, 0x00, 13600);   // master new RAM = white                                                                       
  set_ram_master_();                                                                                                                 
  write_ram_(0x26, 0x00, 13600);   // master old RAM = white                                                                       
  set_ram_slave_();                                                                                                                  
  write_ram_(0xA4, 0x00, 13600);   // slave new RAM = white
  set_ram_slave_();                                                                                                                  
  write_ram_(0xA6, 0x00, 13600);   // slave old RAM = white

    // Full refresh to flush panel to known-white state                                                                              
  send_command_(0x22);                                                                                                               
  send_data_(0xF7);                                                                                                                  
  send_command_(0x20);                                                                                                               
  this->wait_busy_();                                                                                                                
                  
  ESP_LOGI(TAG, "Init complete");
}     

}  // namespace crowpanel_579
}  // namespace esphome