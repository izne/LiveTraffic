//
//  TextIO.cpp
//  LiveTraffic

/*
 * Copyright (c) 2018, Birger Hoppe
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "LiveTraffic.h"

// MARK: LiveTraffic Exception classes

// standard constructor
LTError::LTError (const char* _szFile, int _ln, const char* _szFunc,
                  logLevelTy _lvl,
                  const char* _szMsg, ...) :
fileName(_szFile), ln(_ln), funcName(_szFunc),
lvl(_lvl),
std::logic_error(GetLogString(_szFile, _ln, _szFunc, _lvl, _szMsg, NULL))
{
    va_list args;
    va_start (args, _szMsg);
    msg = GetLogString(_szFile, _ln, _szFunc, _lvl, _szMsg, args);
    va_end (args);
    
    // write to log (flushed immediately -> expensive!)
    if (_lvl >= dataRefs.GetLogLevel())
        XPLMDebugString ( msg.c_str() );
}

// protected constructor, only called by LTErrorFD
LTError::LTError (const char* _szFile, int _ln, const char* _szFunc,
                  logLevelTy _lvl) :
fileName(_szFile), ln(_ln), funcName(_szFunc),
lvl(_lvl),
std::logic_error(GetLogString(_szFile, _ln, _szFunc, _lvl, "", NULL))
{}

const char* LTError::what() const noexcept
{
    return msg.c_str();
}

// includsive reference to LTFlightData
LTErrorFD::LTErrorFD (LTFlightData& _fd,
                      const char* _szFile, int _ln, const char* _szFunc,
                      logLevelTy _lvl,
                      const char* _szMsg, ...) :
fd(_fd),
posStr(_fd.Positions2String()),
LTError(_szFile,_ln,_szFunc,_lvl)
{
    va_list args;
    va_start (args, _szMsg);
    msg = GetLogString(_szFile, _ln, _szFunc, _lvl, _szMsg, args);
    va_end (args);
    
    // write to log (flushed immediately -> expensive!)
    if (_lvl >= dataRefs.GetLogLevel()) {
        XPLMDebugString ( msg.c_str() );
        XPLMDebugString ( posStr.c_str() );
    }
}


//MARK: custom X-Plane message Window - Globals

// An opaque handle to the window we will create
XPLMWindowID    g_window = 0;

// time until window is to be shown, will be destroyed after that
struct dispTextTy {
    float fTimeDisp;                        // until when to display this line?
    logLevelTy lvlDisp;                     // level of msg (defines text color)
    std::string text;                       // text of line
};
std::list<dispTextTy> listTexts;     // lines of text to be displayed

float COL_LVL[logMSG+1][3] = {          // text colors [RGB] depending on log level
    {0.00f, 0.00f, 0.00f},              // 0
    {1.00f, 1.00f, 1.00f},              // INFO (white)
    {1.00f, 1.00f, 0.00f},              // WARN (yellow)
    {1.00f, 0.00f, 0.00f},              // ERROR (red)
    {0.63f, 0.13f, 0.94f},              // FATAL (purple)
    {1.00f, 1.00f, 1.00f}               // MSG (white)
};

//MARK: custom X-Plane message Window - Private Callbacks
// Callbacks we will register when we create our window
void    draw_msg(XPLMWindowID in_window_id, void * /*in_refcon*/)
{
    // Mandatory: We *must* set the OpenGL state before drawing
    // (we can't make any assumptions about it)
    XPLMSetGraphicsState(
                         0 /* no fog */,
                         0 /* 0 texture units */,
                         0 /* no lighting */,
                         0 /* no alpha testing */,
                         1 /* do alpha blend */,
                         1 /* do depth testing */,
                         0 /* no depth writing */
                         );
    
    int l, t, r, b;
    XPLMGetWindowGeometry(in_window_id, &l, &t, &r, &b);
    
    XPLMDrawTranslucentDarkBox(l, t, r, b);
    
    b = WIN_WIDTH;                          // word wrap width = window width
    
    // for each line of text to be displayed
    t -= WIN_ROW_HEIGHT;                    // move down to text's baseline
    for (auto iter = listTexts.cbegin();
         iter != listTexts.cend();
         t -= 2*WIN_ROW_HEIGHT)             // can't deduce number of rwos (after word wrap)...just assume 2 rows are enough
    {
        // still a valid entry?
        if (iter->fTimeDisp > 0 &&
            dataRefs.GetTotalRunningTimeSec() <= iter->fTimeDisp)
        {
            // draw text, take color based on msg level
            XPLMDrawString(COL_LVL[iter->lvlDisp], l, t,
                           const_cast<char*>(iter->text.c_str()),
                           &b, xplmFont_Proportional);
            // next element
            iter++;
        }
        else {
            // now outdated. Move on to next line, but remove this one
            auto iterRemove = iter++;
            listTexts.erase(iterRemove);
        }
    }
    
    // No texts left? Remove window
    if ((g_window == in_window_id) &&
        listTexts.empty())
    {
        DestroyWindow();
    }
}

int dummy_mouse_handler(XPLMWindowID /*in_window_id*/, int /*x*/, int /*y*/, int /*is_down*/, void * /*in_refcon*/)
{ return 0; }

XPLMCursorStatus dummy_cursor_status_handler(XPLMWindowID /*in_window_id*/, int /*x*/, int /*y*/, void * /*in_refcon*/)
{ return xplm_CursorDefault; }

int dummy_wheel_handler(XPLMWindowID /*in_window_id*/, int /*x*/, int /*y*/, int /*wheel*/, int /*clicks*/, void * /*in_refcon*/)
{ return 0; }

void dummy_key_handler(XPLMWindowID /*in_window_id*/, char /*key*/, XPLMKeyFlags /*flags*/, char /*virtual_key*/, void * /*in_refcon*/, int /*losing_focus*/)
{ }


//MARK: custom X-Plane message Window - Create / Destroy
XPLMWindowID CreateMsgWindow(float fTimeToDisplay, logLevelTy lvl, const char* szMsg, ...)
{
    va_list args;

    // save the text in a static buffer queried by the drawing callback
    char aszMsgTxt[500];
    va_start (args, szMsg);
    vsnprintf(aszMsgTxt,
              sizeof(aszMsgTxt),
              szMsg,
              args);
    va_end (args);
    
    // define the text to display:
    dispTextTy dispTxt = {
        // set the timer if a limit is given
        fTimeToDisplay ? dataRefs.GetTotalRunningTimeSec() + fTimeToDisplay : 0,
        // log level to define the color
        lvl,
        // finally the text
        aszMsgTxt
    };
    
    // add to list of display texts
    listTexts.emplace_back(std::move(dispTxt));
    
    // Otherwise: Create the message window
    XPLMCreateWindow_t params;
    params.structSize = sizeof(params);
    params.visible = 1;
    params.drawWindowFunc = draw_msg;
    // Note on "dummy" handlers:
    // Even if we don't want to handle these events, we have to register a "do-nothing" callback for them
    params.handleMouseClickFunc = dummy_mouse_handler;
#if defined(XPLM300)
    params.handleRightClickFunc = dummy_mouse_handler;
#endif
    params.handleMouseWheelFunc = dummy_wheel_handler;
    params.handleKeyFunc = dummy_key_handler;
    params.handleCursorFunc = dummy_cursor_status_handler;
    params.refcon = NULL;
#if defined(XPLM300)
    params.layer = xplm_WindowLayerFloatingWindows;
#endif
#if defined(XPLM301)
    // Opt-in to styling our window like an X-Plane 11 native window
    // If you're on XPLM300, not XPLM301, swap this enum for the literal value 1.
    params.decorateAsFloatingWindow = xplm_WindowDecorationRoundRectangle;
#endif
    
    // Set the window's initial bounds
    // Note that we're not guaranteed that the main monitor's lower left is at (0, 0)...
    // We'll need to query for the global desktop bounds!
#if defined(XPLM300)
    XPLMGetScreenBoundsGlobal(&params.left, &params.top, &params.right, &params.bottom);
#else
    params.left = 0;
    XPLMGetScreenSize(&params.right,&params.top);
    params.bottom = 0;
#endif
    
    // define a window in the top right corner,
    // WIN_FROM_TOP point down from the top, WIN_WIDTH points wide,
    // enough height for all lines of text
    params.top -= WIN_FROM_TOP;
    params.right -= WIN_FROM_RIGHT;
    params.left = params.right - WIN_WIDTH;
    params.bottom = params.top - (WIN_ROW_HEIGHT * (2*int(listTexts.size())+1));
    
    // if the window still exists just resize it
    if (g_window)
        XPLMSetWindowGeometry(g_window, params.left, params.top, params.right, params.bottom);
    else
        // otherwise create a new one
        g_window = XPLMCreateWindowEx(&params);
    
    if ( !g_window ) return NULL;
    
#if defined(XPLM300)
    // Position the window as a "free" floating window, which the user can drag around
    XPLMSetWindowPositioningMode(window, xplm_WindowPositionFree, -1);
    // Limit resizing our window: maintain a minimum width/height of 100 boxels and a max width/height of 300 boxels
    XPLMSetWindowResizingLimits(window, 200, 200, 300, 300);
    XPLMSetWindowTitle(window, LIVE_TRAFFIC);
#endif
    
//    LOG_MSG(logDEBUG, DBG_WND_CREATED_UNTIL, dispTxt.fTimeDisp, aszMsgTxt);
    
    return g_window;
}


void DestroyWindow()
{
    if ( g_window )
    {
        XPLMDestroyWindow(g_window);
        g_window = NULL;
        listTexts.clear();
//        LOG_MSG(logDEBUG,DBG_WND_DESTROYED);
   }
}

//
//MARK: Log
//

const char* LOG_LEVEL[] = {
    "DEBUG", "INFO ", "WARN ", "ERROR", "FATAL", "MSG  "
};

// returns ptr to static buffer filled with log string
const char* GetLogString (const char* szPath, int ln, const char* szFunc,
                          logLevelTy lvl, const char* szMsg, va_list args )
{
    static char aszMsg[2048];
    const double simTime = dataRefs.GetSimTime();

    // prepare timestamp
    if ( lvl < logMSG )                             // normal messages without, all other with location info
    {
        const char* szFile = strrchr(szPath,'/');   // extract file from path
        if ( !szFile ) szFile = szPath; else szFile++;
        snprintf(aszMsg, sizeof(aszMsg), "%s %.1f %s %s:%d/%s: ",
                LIVE_TRAFFIC, simTime, LOG_LEVEL[lvl],
                szFile, ln, szFunc);
    }
    else
        snprintf(aszMsg, sizeof(aszMsg), "%s: ", LIVE_TRAFFIC);
    
    // append given message
    if (args) {
        vsnprintf(&aszMsg[strlen(aszMsg)],
                  sizeof(aszMsg)-strlen(aszMsg)-1,      // we save one char for the CR
                  szMsg,
                  args);
    }

    // ensure there's a trailing CR
    size_t l = strlen(aszMsg);
    if ( aszMsg[l-1] != '\n' )
    {
        aszMsg[l]   = '\n';
        aszMsg[l+1] = 0;
    }

    // return the static buffer
    return aszMsg;
}

void LogMsg ( const char* szPath, int ln, const char* szFunc, logLevelTy lvl, const char* szMsg, ... )
{
    va_list args;

    va_start (args, szMsg);
    // write to log (flushed immediately -> expensive!)
    XPLMDebugString ( GetLogString(szPath, ln, szFunc, lvl, szMsg, args) );
    va_end (args);
}
