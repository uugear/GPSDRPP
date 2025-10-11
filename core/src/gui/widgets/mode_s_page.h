#pragma once
#include <gui/widgets/side_bar.h>
#include <mode_s_decoder.h>
#include <string>


class ModeSPage : public SideBar::SidePage {
public:
	static ModeSPage& getInstance() {
        static ModeSPage instance;
        return instance;
    }
	ModeSPage(const ModeSPage&) = delete;
    ModeSPage& operator=(const ModeSPage&) = delete;
	
	const char* getLabel();
	void init();
	void deinit();
    void draw();
	void addLog(const std::string& message);
	void processMessage(struct mode_s_message * mm);
	struct mode_s_context * getContext();
	bool isRunning();
	
private:
	ModeSPage();
    ~ModeSPage();
	
	struct mode_s_context ctx;
	
	bool running;
	std::string log_content;
};
