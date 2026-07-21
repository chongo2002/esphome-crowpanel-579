FROM mcr.microsoft.com/playwright/python:v1.45.0-jammy

WORKDIR /app

COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt \
    && python3 -m playwright install --with-deps chromium

COPY render.py run.sh ./
RUN chmod +x run.sh

CMD ["./run.sh"]
