idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES certs/client/telegramservercert.pem
                    certs/server/servercert.pem certs/server/prvtkey.pem)
