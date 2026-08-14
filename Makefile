CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter \
           $(shell sdl2-config --cflags) \
           -Ithird_party/imgui -Ithird_party/imgui/backends -Ithird_party/stb \
           -Ithird_party/ImGuiFileDialog \
           -DIMGUI_DISABLE_OBSOLETE_FUNCTIONS
LIBS     = $(shell sdl2-config --libs)

IMGUI_DIR = third_party/imgui
IMGUI_SRC = $(IMGUI_DIR)/imgui.cpp \
            $(IMGUI_DIR)/imgui_draw.cpp \
            $(IMGUI_DIR)/imgui_tables.cpp \
            $(IMGUI_DIR)/imgui_widgets.cpp \
            $(IMGUI_DIR)/backends/imgui_impl_sdl2.cpp \
            $(IMGUI_DIR)/backends/imgui_impl_sdlrenderer2.cpp \
            third_party/ImGuiFileDialog/ImGuiFileDialog.cpp

APP_SRC  = src/main.cpp \
           src/app.cpp \
           src/sprite.cpp \
           src/project.cpp \
           src/packer.cpp \
           src/validation.cpp \
           src/report.cpp \
           src/sizes.cpp \
           src/exporter.cpp \
           src/ui.cpp \
           src/stb_impl.cpp

SRC      = $(IMGUI_SRC) $(APP_SRC)
OBJDIR   = build
OBJS     = $(SRC:%.cpp=$(OBJDIR)/%.o)
DEPS     = $(OBJS:.o=.d)
TARGET   = spritesheet

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

$(OBJDIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(DEPS)

clean:
	rm -rf $(OBJDIR) $(TARGET)

.PHONY: clean
