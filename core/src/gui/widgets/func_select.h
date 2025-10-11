#pragma once

namespace ImGui {
    
	extern const char *functions[];
	extern const int functions_count;
	
	class FunctionSelect {
    public:
		FunctionSelect(const char * identifier, int posX, int posY, int select);
		bool isVisible();
		void setVisible(bool show);
		void draw();
		int getSel();
	private:
		char * id;
		int x;
		int y;
		bool visible = false;
		int highlight;
		int sel;
		bool newly_shown = false;
		float popup_start_time;
		float auto_close_delay;
	};
};
