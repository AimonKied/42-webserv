CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Werror -Iinclude
SRC_DIR = src
OBJ_DIR = obj
SRC = $(wildcard $(SRC_DIR)/*.cpp)
OBJ = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRC))
TARGET = webserv

all: $(OBJ_DIR) $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ) $(OBJ_DIR)

fclean: clean
	rm -f $(TARGET)

re: fclean all



.PHONY: all clean fclean rebuild
