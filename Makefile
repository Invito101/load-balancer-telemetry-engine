CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -O2 -g
LDFLAGS := -pthread

BIN_DIR := bin
SRC_DIR := src

COMMON := $(SRC_DIR)/common.c

all: $(BIN_DIR)/lb_server $(BIN_DIR)/lb_client $(BIN_DIR)/backend_worker $(BIN_DIR)/audit_logger

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BIN_DIR)/lb_server: $(BIN_DIR) $(SRC_DIR)/lb_server.c $(SRC_DIR)/lb_server.h $(SRC_DIR)/lb_server_state.c $(SRC_DIR)/lb_server_auth.c $(SRC_DIR)/lb_server_runtime.c $(COMMON) $(SRC_DIR)/common.h
	$(CC) $(CFLAGS) $(SRC_DIR)/lb_server.c $(SRC_DIR)/lb_server_state.c $(SRC_DIR)/lb_server_auth.c $(SRC_DIR)/lb_server_runtime.c $(COMMON) -o $@ $(LDFLAGS)

$(BIN_DIR)/lb_client: $(BIN_DIR) $(SRC_DIR)/lb_client.c $(COMMON) $(SRC_DIR)/common.h
	$(CC) $(CFLAGS) $(SRC_DIR)/lb_client.c $(COMMON) -o $@ $(LDFLAGS)

$(BIN_DIR)/backend_worker: $(BIN_DIR) $(SRC_DIR)/backend_worker.c $(COMMON) $(SRC_DIR)/common.h
	$(CC) $(CFLAGS) $(SRC_DIR)/backend_worker.c $(COMMON) -o $@ $(LDFLAGS)

$(BIN_DIR)/audit_logger: $(BIN_DIR) $(SRC_DIR)/audit_logger.c $(COMMON) $(SRC_DIR)/common.h
	$(CC) $(CFLAGS) $(SRC_DIR)/audit_logger.c $(COMMON) -o $@ $(LDFLAGS)

clean:
	rm -rf $(BIN_DIR)

.PHONY: all clean