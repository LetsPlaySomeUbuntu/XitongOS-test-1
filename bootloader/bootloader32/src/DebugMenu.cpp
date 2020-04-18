#include "DebugMenu.hpp"

#include <FunnyOS/BootloaderCommons/Logging.hpp>
#include <FunnyOS/Stdlib/Memory.hpp>
#include <FunnyOS/Stdlib/String.hpp>
#include <FunnyOS/Hardware/CPU.hpp>

#include "DebugModeOptions.hpp"

namespace FunnyOS::Bootloader32::DebugMenu {
    using namespace Stdlib;
    using namespace Misc::TerminalManager;
    using HW::PS2::ScanCode;

    namespace {
        constexpr ScanCode MENU_ENTER_KEY = ScanCode::Z_Released;
        constexpr ScanCode MENU_EXIT_KEY = ScanCode::Escape_Released;

        constexpr int MENU_ENTER_KEY_REPEAT = 3;

        unsigned int g_menuEnterKeyPressedCount = 0;
        unsigned int g_currentSelection = 0;
        bool g_menuEnabled = false;
        bool g_exitRequested = false;

        void DrawFramePart(char sides, char center) {
            auto* terminalManager = Bootloader::Logging::GetTerminalManager();
            const size_t screenWidth = terminalManager->GetInterface()->GetScreenWidth();

            String::StringBuffer buffer = Memory::AllocateBuffer<char>(screenWidth + 1);
            *buffer[0] = sides;
            *buffer[screenWidth - 1] = sides;

            for (size_t i = 1; i < screenWidth - 1; i++) {
                *buffer[i] = center;
            }
            *buffer[screenWidth] = 0;

            terminalManager->PrintString(buffer.Data);
            Memory::FreeBuffer(buffer);
        }

        void PrintHeader(const char* header) {
            auto* terminalManager = Bootloader::Logging::GetTerminalManager();
            terminalManager->ClearScreen();
            DrawFramePart('+', '=');
            DrawFramePart('|', ' ');
            DrawFramePart('+', '=');

            const size_t screenWidth = terminalManager->GetInterface()->GetScreenWidth();
            const size_t headerLength = String::Length(header);
            const auto xPosition = static_cast<uint8_t>(screenWidth / 2 - headerLength / 2);
            terminalManager->GetInterface()->SetCursorPosition({xPosition, 1});
            terminalManager->PrintString(header);
            terminalManager->GetInterface()->SetCursorPosition({0, 4});
        }

        void PrintOption(const MenuOption& option) {
            char noState[] = "";
            auto* terminalManager = Bootloader::Logging::GetTerminalManager();
            const size_t screenWidth = terminalManager->GetInterface()->GetScreenWidth();
            String::StringBuffer buffer = Memory::AllocateBuffer<char>(screenWidth + 1);
            String::Concat(buffer, " * ", option.Name);

            char* state = option.FetchState == nullptr ? noState : option.FetchState();

            const size_t optionNameLength = String::Length(buffer.Data);
            const size_t stateLength = String::Length(state);

            for (size_t i = optionNameLength; i < screenWidth - stateLength; i++) {
                *buffer[i] = ' ';
            }

            Memory::Copy(buffer.Data + screenWidth - stateLength, state, stateLength);
            if (option.FetchState != nullptr) {
                Memory::Free(state);
            }
            buffer.Data[buffer.Size - 1] = 0;
            terminalManager->PrintString(buffer.Data);

            Memory::FreeBuffer(buffer);
        }

        void DrawMenu() {
            auto* terminalManager = Bootloader::Logging::GetTerminalManager();
            PrintHeader("FunnyOS v" FUNNYOS_VERSION " Debug menu");
            terminalManager->PrintLine();

            for (size_t i = 0; i < sizeof(g_menuOptions) / sizeof(g_menuOptions[0]); i++) {
                const auto& option = g_menuOptions[i];

                if (g_currentSelection == i) {
                    terminalManager->ChangeColor(Color::LightGray, Color::Black);
                }

                PrintOption(option);
                terminalManager->ChangeColor(Color::Black, Color::White);
            }
        }
    }  // namespace

    void HandleKey(ScanCode code) {
        if (code == MENU_ENTER_KEY) {
            g_menuEnterKeyPressedCount++;
            return;
        }

        if (!g_menuEnabled) {
            return;
        }

        if (code == MENU_EXIT_KEY) {
            g_exitRequested = true;
            return;
        }

        constexpr const size_t optionsCount = sizeof(g_menuOptions) / sizeof(g_menuOptions[0]);

        if (code == ScanCode::CursorDown_Pressed && g_currentSelection < optionsCount - 1) {
            g_currentSelection++;
        } else if (code == ScanCode::CursorUp_Pressed && g_currentSelection > 0) {
            g_currentSelection--;
        } else if (code == ScanCode::Enter_Pressed) {
            g_menuOptions[g_currentSelection].Handle();
        } else {
            return;
        }

        DrawMenu();
    }

    bool MenuRequested() {
        return g_menuEnterKeyPressedCount >= MENU_ENTER_KEY_REPEAT;
    }

    void Enter() {
        FB_LOG_INFO("Entering debug menu...");
        g_menuEnabled = true;

        auto terminalManager = Bootloader::Logging::GetTerminalManager();
        auto savedScreenData = terminalManager->GetInterface()->SaveScreenData();
        terminalManager->ClearScreen();
        DrawMenu();

        while (!g_exitRequested) {
            HW::CPU::Halt();
        }

        terminalManager->GetInterface()->RestoreScreenData(savedScreenData);
        g_menuEnabled = false;
        FB_LOG_OK("Debugging menu exited successfully!");
        FB_LOG_DEBUG("Debug mode is enabled");
    }
}  // namespace FunnyOS::Bootloader32::DebugMenu