/*
 * Platformer Game Engine by Wohlstand, a free platform for game making
 * Copyright (c) 2016 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <ctime>
#include <iostream>
#ifdef _WIN32
#include <windows.h>
#endif

#include <QDir>
#include <QFileInfo>

#include "engine.hpp"

#include <version.h>
#include <graphics/window.h>

#include <audio/pge_audio.h>

#include <common_features/crash_handler.h>
#include <common_features/logger.h>
#include <common_features/translator.h>
#include <common_features/number_limiter.h>

#include <data_configs/config_manager.h>

#include <fontman/font_manager.h>

#include <settings/global_settings.h>
#include <settings/debugger.h>

#include <networking/intproc.h>

PGEEngineCmdArgs    g_flags;

Level_returnTo      g_jumpOnLevelEndTo = RETURN_TO_EXIT;

QString             g_configPackPath  = "";
QString             g_fileToOpen  = "";
EpisodeState        g_GameState;
PlayEpisodeResult   g_Episode;

PGEEngineCmdArgs::PGEEngineCmdArgs() :
    debugMode(false),
    audioEnabled(true),
    testWorld(false),
    testLevel(false),
    test_NumPlayers(1),
    test_Characters{ -1, -1, -1, -1 },
    test_States{ 1, 1, 1, 1 },
    rendererType(GlRenderer::RENDER_AUTO)
{}



PGEEngineApp::PGEEngineApp() :
    m_lib(NOTHING),
    m_qApp(nullptr),
    m_tr(nullptr)
{
    CrashHandler::initSigs();
    srand(static_cast<unsigned int>(std::time(NULL)));
}

PGEEngineApp::~PGEEngineApp()
{
    unloadAll();
}

void PGEEngineApp::unloadAll()
{
    if(enabled(CONFIG_MANAGER))
    {
        ConfigManager::unluadAll();
        disable(CONFIG_MANAGER);
    }

    if(enabled(AUDIO_ENGINE))
    {
        PGE_Audio::quit();
        disable(AUDIO_ENGINE);
    }

    if(enabled(SETTINGS_LOADED))
    {
        g_AppSettings.save();
        disable(SETTINGS_LOADED);
    }

    if(enabled(JOYSTICKS))
    {
        g_AppSettings.closeJoysticks();
        disable(JOYSTICKS);
    }

    if(enabled(INTERPROCESSOR))
    {
        if(IntProc::isEnabled())
            IntProc::quit();

        disable(INTERPROCESSOR);
    }

    if(enabled(FONT_MANAGER))
    {
        FontManager::quit();
        disable(FONT_MANAGER);
    }

    if(enabled(PGE_WINDOW))
    {
        PGE_Window::uninit();
        disable(PGE_WINDOW);
    }

    if(enabled(LIBSDL))
    {
        SDL_Quit();
        disable(LIBSDL);
    }

    if(enabled(TRANSLATOR))
    {
        delete m_tr;
        m_tr = nullptr;
        disable(TRANSLATOR);
    }

    pLogDebug("<Application closed>");

    if(enabled(QAPP))
    {
        m_qApp->quit();
        m_qApp->exit();
        delete m_qApp;
        m_qApp = nullptr;
        disable(QAPP);
    }

    if(enabled(LOGGER))
    {
        CloseLog();
        disable(LOGGER);
    }
}

PGE_Application *PGEEngineApp::loadQApp(int argc, char **argv)
{
    PGE_Application::addLibraryPath(".");
    PGE_Application::addLibraryPath(QFileInfo(QString::fromUtf8(argv[0])).dir().path());
    PGE_Application::addLibraryPath(QFileInfo(QString::fromLocal8Bit(argv[0])).dir().path());
    PGE_Application::setAttribute(Qt::AA_Use96Dpi);
    m_qApp = new PGE_Application(argc, argv);

    if(!m_qApp)
    {
        pLogFatal("Can't construct application class!");
        return nullptr;
    }

    m_args = m_qApp->arguments();
    //Generating application path
    //Init system paths
    AppPathManager::initAppPath();
    enable(QAPP);
    return m_qApp;
}

PGE_Translator &PGEEngineApp::loadTr()
{
    pLogDebug("Constructing translator...");
    m_tr = new PGE_Translator();
    pLogDebug("Initializing translator...");
    m_tr->init();
    enable(TRANSLATOR);
    pLogDebug("Translator successfully built...");
    return *m_tr;
}

void PGEEngineApp::initInterprocessor()
{
    IntProc::init();
    enable(INTERPROCESSOR);
}

bool PGEEngineApp::initSDL()
{
    bool res = false;
    pLogDebug("Initialization of SDL...");
    Uint32 sdlInitFlags = 0;
    // Prepare flags for SDL initialization
    sdlInitFlags |= SDL_INIT_TIMER;
    sdlInitFlags |= SDL_INIT_AUDIO;
    sdlInitFlags |= SDL_INIT_VIDEO;
    sdlInitFlags |= SDL_INIT_EVENTS;
    sdlInitFlags |= SDL_INIT_JOYSTICK;
    //(Cool thing, but is not needed yet)
    //sdlInitFlags |= SDL_INIT_HAPTIC;
    sdlInitFlags |= SDL_INIT_GAMECONTROLLER;
    // Initialize SDL
    res = (SDL_Init(sdlInitFlags) < 0);

    if(!res)
        enable(LIBSDL);

    return res;
}

bool PGEEngineApp::initAudio(bool useAudio)
{
    bool ret = false;

    if(!useAudio)
        return true;

    pLogDebug("Initialization of Audio subsystem...");
    ret = (PGE_Audio::init(44100, 32, 4096) == -1);

    if(!ret)
        enable(AUDIO_ENGINE);
    else
        m_audioError = Mix_GetError();

    return ret;
}

std::string PGEEngineApp::errorAudio()
{
    return m_audioError;
}

bool PGEEngineApp::initWindow(std::string title, int renderType)
{
    bool ret = true;
    pLogDebug("Init main window...");
    ret = PGE_Window::init(title, renderType);

    if(ret)
        enable(PGE_WINDOW);

    return !ret;
}

void PGEEngineApp::initFontBasics()
{
    pLogDebug("Init basic font manager...");
    FontManager::initBasic();
    enable(FONT_MANAGER);
}

void PGEEngineApp::initFontFull()
{
    pLogDebug("Init full font manager...");
    FontManager::initFull();
    enable(FONT_MANAGER);
}

void PGEEngineApp::loadSettings()
{
    g_AppSettings.load();
    g_AppSettings.apply();
    enable(SETTINGS_LOADED);
}

void PGEEngineApp::loadJoysticks()
{
    pLogDebug("Init joystics...");
    g_AppSettings.initJoysticks();
    g_AppSettings.loadJoystickSettings();
    enable(JOYSTICKS);
}

void PGEEngineApp::loadLogger()
{
    LoadLogSettings();
    //Write into log the application start event
    pLogDebug("<Application started>");
    enable(LOGGER);
}

static void printUsage(char *arg0)
{
    std::string arg0s(arg0);
    #ifndef _WIN32
    const char* logo =
        "=================================================\n"
        INITIAL_WINDOW_TITLE "\n"
        "=================================================\n"
        "\n";
    #define OPTIONAL_BREAK "\n"
    #else
    #define OPTIONAL_BREAK " "
    #endif

    std::string msg(
        "Command line syntax:\n"
        "\n"
        "Start game with a specific config pack:\n"
        "   " + arg0s + " [options] --config=\"./configs/Config Pack Name/\"\n"
        "\n"
        "Play single level or episode:\n"
        "   " + arg0s + " [options] <path to level or world map file>\n"
        "\n"
        #ifndef _WIN32
        "Show application version:\n"
        "   " + arg0s + " --version\n"
        "\n"
        #endif
        "Copy settings into "
        #if defined(_WIN32)
        "%UserProfile%/.PGE_Project/"
        #elif defined(__APPLE__)
        "/Library/Application Support/PGE_Project/"
        #else
        "~/.PGE_Project/"
        #endif
        " folder and use it as" OPTIONAL_BREAK
        "placement of config packs, episodes, for screenshots store, etc.:\n"
        "   " + arg0s + " --install\n"
        "\n"
        #ifndef _WIN32
        "Show this help:\n"
        "   " + arg0s + " --help\n"
        "\n\n"
        #endif
        "Options:\n\n"
        "  --config=\"{path}\"          - Use a specific configuration package\n"
        #if defined(__APPLE__)
        "  --render-[auto|sw|gl2|gl3] - Choose a graphical sub-system\n"
        #else
        "  --render-[auto|sw|gl2|gl3] - Choose a graphical sub-system\n"
        #endif
        "             auto - Automatically detect it (DEFAULT)\n"
        #if !defined(__APPLE__)
        "             gl3  - Use OpenGL 3.1 renderer\n"
        #endif
        "             gl2  - Use OpenGL 2.1 renderer\n"
        "             sw   - Use software renderer (may overload CPU)\n"
        "  --render-vsync             - Toggle on VSync if supported on your hardware\n"
        "  --lang=xx                  - Use a specific language (default is locale)\n"
        "            (where xx - a code of language. I.e., en, ru, es, nl, pl, etc.)\n"
        "  --num-players=X            - Start game with X number of players\n"
        "  --pXc=Y                    - Set character Y for player X\n"
        "            (for example --p1c=2 will set character with ID 2 to first player)\n"
        "  --pXs=Y                    - Set character state Y for player X\n"
        "            (for example --p2s=4 will set state with ID 4 to second player)\n"
        "  --debug                    - Enable debug mode\n"
        "  --debug-physics            - Enable debug rendering of the physics\n"
        "  --debug-print=[yes|no]     - Enable/Disable debug information printing\n"
        "  --debug-pagan-god          - Enable god mode\n"
        "  --debug-superman           - Enable unlimited flying up\n"
        "  --debug-chucknorris        - Allow to playable character destroy any objects\n"
        "  --debug-worldfreedom       - Allow to walk everywhere on the world map\n"
        "\n"
        "More detailed information can be found here:\n"
        "http://wohlsoft.ru/pgewiki/PGE_Engine#Command_line_arguments\n"
        OPTIONAL_BREAK);

    #ifdef _WIN32
    MessageBoxA(NULL, msg.c_str(), INITIAL_WINDOW_TITLE, MB_OK | MB_ICONINFORMATION);
    #else
    fprintf(stderr, "%s%s", logo, msg.c_str());
    #endif
}

bool PGEEngineApp::parseLowArgs(int argc, char **argv)
{
    if(argc > 1)
    {
        //Check only first argument
        char *arg = argv[1];

        if(strcmp(arg, "--version") == 0)
        {
            std::cout << _INTERNAL_NAME " " _FILE_VERSION << _FILE_RELEASE "-" _BUILD_VER << std::endl;
            std::cout.flush();
            return true;
        }
        else if(strcmp(arg, "--install") == 0)
        {
            PGEEngineApp  lib;
            lib.loadQApp(argc, argv);
            AppPathManager::install();
            AppPathManager::initAppPath();
            return true;
        }
        else if(strcmp(arg, "--help") == 0)
        {
            printUsage(argv[0]);
            return true;
        }
    }

    return false;
}

static void removeQuotes(char *&s, size_t &len)
{
    if(len > 0)
    {
        if(s[0] == '\"')
        {
            s++;
            len--;
        }

        if((len > 0) && (s[len - 1] == '\"'))
        {
            s[len - 1] = '\0';
            len--;
        }
    }
}

static void findEqual(char *&s, size_t &len)
{
    while(len > 0)
    {
        len--;
        if(*s++ == '=')
            break;
    }
}

static QString takeStrFromArg(std::string &arg, bool &ok)
{
    std::string target = arg;
    size_t      len = arg.size();
    char        *s = &target[0];

    findEqual(s, len);
    removeQuotes(s, len);
    ok = (len > 0);

    return QString::fromUtf8(s, (int)len);
}

static int takeIntFromArg(std::string &arg, bool &ok)
{
    std::string target = arg;
    size_t  len = arg.size();
    char    *s  = &target[0];

    findEqual(s, len);
    removeQuotes(s, len);

    ok = (len > 0);

    return atoi(s);
}

void PGEEngineApp::parseHighArgs()
{
    /* Set defaults to global properties */
    g_Episode.character = 0;
    g_Episode.savefile  = "save1.savx";
    g_Episode.worldfile = "";
    g_AppSettings.debugMode         = false; //enable debug mode
    g_AppSettings.interprocessing   = false; //enable interprocessing

    for(int pi = 1; pi < m_args.size(); pi++)
    {
        QString &param = m_args[pi];
        std::string param_s = param.toStdString();
        pLogDebug("Argument: [%s]", param_s.c_str());
        int i = 0;
        char characterParam[8] = "\0";
        char stateParam[8] = "\0";

        for(i = 0; i < 4; i++)
        {
            sprintf(characterParam, "--p%dc=", i + 1);
            sprintf(stateParam, "--p%ds=", i + 1);

            if(param_s.compare(0, 6, characterParam) == 0)
            {
                int tmp;
                bool ok = false;
                tmp = takeIntFromArg(param_s, ok);

                if(ok) g_flags.test_Characters[i] = tmp;

                break;
            }
            else if(param_s.compare(0, 6, stateParam) == 0)
            {
                int tmp;
                bool ok = false;
                tmp = takeIntFromArg(param_s, ok);

                if(ok)
                {
                    g_flags.test_States[i] = tmp;

                    if(g_flags.test_Characters[i] == -1)
                        g_flags.test_Characters[i] = 1;
                }

                break;
            }
        }

        if(i < 4) //If one of pXc/pYs parameters has been detected
            continue;

        if(param_s.compare(0, 9, "--config=") == 0)
        {
            QString tmp;
            bool ok = false;
            tmp = takeStrFromArg(param_s, ok);

            if(ok) g_configPackPath = tmp;
        }
        else if(param_s.compare(0, 14, "--num-players=") == 0)
        {
            int tmp;
            bool ok = false;
            tmp = takeIntFromArg(param_s, ok);

            if(ok) g_flags.test_NumPlayers = tmp;

            //1 or 2 players until 4-players mode will be implemented!
            NumberLimiter::applyD(g_flags.test_NumPlayers, 1, 1, 2);
        }
        else if(param_s.compare("--debug") == 0)
        {
            g_AppSettings.debugMode = true;
            g_flags.debugMode = true;
        }
        else if(param_s.compare("--debug-pagan-god") == 0)
        {
            if(g_flags.debugMode)
                PGE_Debugger::cheat_pagangod = true;
        }
        else if(param_s.compare("--debug-superman") == 0)
        {
            if(g_flags.debugMode)
                PGE_Debugger::cheat_superman = true;
        }
        else if(param_s.compare("--debug-chucknorris") == 0)
        {
            if(g_flags.debugMode)
                PGE_Debugger::cheat_chucknorris = true;
        }
        else if(param_s.compare("--debug-worldfreedom") == 0)
        {
            if(g_flags.debugMode)
                PGE_Debugger::cheat_worldfreedom = true;
        }
        else if(param_s.compare("--debug-physics") == 0)
        {
            if(g_flags.debugMode)
                PGE_Window::showPhysicsDebug = true;
        }
        else if(param_s.compare("--debug-print=yes") == 0)
        {
            if(g_flags.debugMode)
            {
                g_AppSettings.showDebugInfo = false;
                PGE_Window::showDebugInfo = true;
            }
        }
        else if(param_s.compare("--debug-print=no") == 0)
        {
            PGE_Window::showDebugInfo = false;
            g_AppSettings.showDebugInfo = false;
        }
        else if(param_s.compare("--interprocessing") == 0)
        {
            initInterprocessor();
            g_AppSettings.interprocessing = true;
        }
        else if(param_s.compare(0, 7, "--lang=") == 0)
        {
            QString tmp;
            bool ok = false;
            tmp = takeStrFromArg(param_s, ok);
            ok &= (tmp.size() == 2);

            if(ok) m_tr->toggleLanguage(tmp);
        }
        else if(param_s.compare("--render-auto") == 0)
            g_flags.rendererType = GlRenderer::RENDER_AUTO;
        else if(param_s.compare("--render-gl2") == 0)
            g_flags.rendererType = GlRenderer::RENDER_OPENGL_2_1;
        else if(param_s.compare("--render-gl3") == 0)
            g_flags.rendererType = GlRenderer::RENDER_OPENGL_3_1;
        else if(param_s.compare("--render-sw") == 0)
            g_flags.rendererType = GlRenderer::RENDER_SW_SDL;
        else if(param_s.compare("--render-vsync") == 0)
        {
            g_AppSettings.vsync = true;
            PGE_Window::vsync = true;
        }
        else
        {
            param = FileFormats::removeQuotes(param);
            param_s = param.toStdString();

            if(QFile::exists(param))
            {
                g_fileToOpen = param;
                pLogDebug("Got file path: [%s]", param_s.c_str());
            }
            else
                pLogWarning("Invalid argument or file path: [%s]", param_s.c_str());
        }
    }

    #ifdef __APPLE__
    if(g_fileToOpen.isEmpty())
    {
        m_qApp->processEvents(QEventLoop::QEventLoop::AllEvents);
        pLogDebug("Attempt to take Finder args...");
        QStringList openArgs = m_qApp->getOpenFileChain();

        foreach(QString file, openArgs)
        {
            if(QFile::exists(file))
            {
                g_fileToOpen = file;
                pLogDebug("Got file path: [%s]", file.toStdString().c_str());
            }
            else
                pLogWarning("Invalid file path, sent by Mac OS X Finder event: [%s]", file.toStdString().c_str());
        }
    }
    #endif
}

void PGEEngineApp::createConfigsDir()
{
    QString configPath = AppPathManager::userAppDir() + "/" +  "configs";
    QDir configDir(configPath);

    //Create empty config directory if not exists
    if(!configDir.exists())
        configDir.mkdir(configPath);
}
