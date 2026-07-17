FROM python:3.12-slim
WORKDIR /srv
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt
COPY app ./app
EXPOSE 8000
# --timeout-keep-alive 75: o device faz ping a cada 30 s pra manter o socket TLS
# quente (reconectar custa ~1,8 s e come o início da fala). O default do uvicorn
# é 5 s — fechava a conexão entre pings e o OK caía em socket frio. 75 s > 30 s
# do ping mantém o socket vivo entre as falas.
CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "8000", "--timeout-keep-alive", "75"]
