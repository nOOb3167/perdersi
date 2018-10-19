import config
import server

if __name__ == "__main__":
    server.server_app.run(host = config["LISTEN_HOST"], port = config["LISTEN_PORT"])
