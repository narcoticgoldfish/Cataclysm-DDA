#if ((!defined TILES) && (defined _WIN32 || defined WINDOWS))
#include "catacurse.h"
#include "options.h"
#include "output.h"
#include "color.h"
#include "catacharset.h"
#include <cstdlib>
#include <fstream>

//***********************************
//Globals                           *
//***********************************

const WCHAR *szWindowClass = (L"CataCurseWindow");    //Class name :D
HINSTANCE WindowINST;   //the instance of the window
HWND WindowHandle;      //the handle of the window
HDC WindowDC;           //Device Context of the window, used for backbuffer
int WindowX;            //X pos of the actual window, not the curses window
int WindowY;            //Y pos of the actual window, not the curses window
int WindowWidth;        //Width of the actual window, not the curses window
int WindowHeight;       //Height of the actual window, not the curses window
int lastchar;          //the last character that was pressed, resets in getch
int inputdelay;         //How long getch will wait for a character to be typed
//WINDOW *_windows;  //Probably need to change this to dynamic at some point
//int WindowCount;        //The number of curses windows currently in use
HDC backbuffer;         //an off-screen DC to prevent flickering, lower cpu
HBITMAP backbit;        //the bitmap that is used in conjunction wth the above
int fontwidth;          //the width of the font, background is always this size
int fontheight;         //the height of the font, background is always this size
int halfwidth;          //half of the font width, used for centering lines
int halfheight;          //half of the font height, used for centering lines
HFONT font;             //Handle to the font created by CreateFont
RGBQUAD *windowsPalette;  //The coor palette, 16 colors emulates a terminal
unsigned char *dcbits;  //the bits of the screen image, for direct access
char szDirectory[MAX_PATH] = "";

//***********************************
//Non-curses, Window functions      *
//***********************************

// declare this locally, because it's not generally cross-compatible in catacurse.h
LRESULT CALLBACK ProcessMessages(HWND__ *hWnd,u_int32_t Msg,WPARAM wParam, LPARAM lParam);

//Registers, creates, and shows the Window!!
bool WinCreate()
{
    int success;
    WNDCLASSEXW WindowClassType;
    int WinBorderHeight;
    int WinBorderWidth;
    int WinTitleSize;
    unsigned int WindowStyle;
    const WCHAR *szTitle=  (L"Cataclysm: Goon Days Ahead - 0.4.3");
    WinTitleSize = GetSystemMetrics(SM_CYCAPTION);      //These lines ensure
    WinBorderWidth = GetSystemMetrics(SM_CXDLGFRAME) * 2;  //that our window will
    WinBorderHeight = GetSystemMetrics(SM_CYDLGFRAME) * 2; // be a perfect size
    WindowClassType.cbSize = sizeof(WNDCLASSEXW);
    WindowClassType.style = 0;//No point in having a custom style, no mouse, etc
    WindowClassType.lpfnWndProc = ProcessMessages;//the procedure that gets msgs
    WindowClassType.cbClsExtra = 0;
    WindowClassType.cbWndExtra = 0;
    WindowClassType.hInstance = WindowINST;// hInstance
    WindowClassType.hIcon = LoadIcon(WindowINST, IDI_APPLICATION);//Default Icon
    WindowClassType.hIconSm = LoadIcon(WindowINST, IDI_APPLICATION);//Default Icon
    WindowClassType.hCursor = LoadCursor(NULL, IDC_ARROW);//Default Pointer
    WindowClassType.lpszMenuName = NULL;
    WindowClassType.hbrBackground = 0;//Thanks jday! Remove background brush
    WindowClassType.lpszClassName = szWindowClass;
    success = RegisterClassExW(&WindowClassType);
    if ( success== 0){
        return false;
    }
    WindowStyle = WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME) & ~(WS_MAXIMIZEBOX);
    WindowHandle = CreateWindowExW(WS_EX_APPWINDOW || WS_EX_TOPMOST * 0,
                                   szWindowClass , szTitle,WindowStyle, WindowX,
                                   WindowY, WindowWidth + (WinBorderWidth) * 1,
                                   WindowHeight + (WinBorderHeight +
                                   WinTitleSize) * 1, 0, 0, WindowINST, NULL);
    if (WindowHandle == 0){
        return false;
    }
    ShowWindow(WindowHandle,5);
    return true;
};

//Unregisters, releases the DC if needed, and destroys the window.
void WinDestroy()
{
    if ((WindowDC > 0) && (ReleaseDC(WindowHandle, WindowDC) == 0)){
        WindowDC = 0;
    }
    if ((!WindowHandle == 0) && (!(DestroyWindow(WindowHandle)))){
        WindowHandle = 0;
    }
    if (!(UnregisterClassW(szWindowClass, WindowINST))){
        WindowINST = 0;
    }
};

//This function processes any Windows messages we get. Keyboard, OnClose, etc
LRESULT CALLBACK ProcessMessages(HWND__ *hWnd,unsigned int Msg,
                                 WPARAM wParam, LPARAM lParam)
{
    switch (Msg){
        case WM_CHAR:               //This handles most key presses
            lastchar=(int)wParam;
            switch (lastchar){
                case 13:            //Reroute ENTER key for compatilbity purposes
                    lastchar=10;
                    break;
                case 8:             //Reroute BACKSPACE key for compatilbity purposes
                    lastchar=127;
                    break;
            };
            break;
        case WM_KEYDOWN:                //Here we handle non-character input
            switch (wParam){
                case VK_LEFT:
                    lastchar = KEY_LEFT;
                    break;
                case VK_RIGHT:
                    lastchar = KEY_RIGHT;
                    break;
                case VK_UP:
                    lastchar = KEY_UP;
                    break;
                case VK_DOWN:
                    lastchar = KEY_DOWN;
                    break;
                case VK_NEXT:
                    lastchar = KEY_NPAGE;
                    break;
                case VK_PRIOR:
                    lastchar = KEY_PPAGE;
                    break;
                default:
                    break;
            };
        case WM_ERASEBKGND:
            return 1;               //We don't want to erase our background
        case WM_PAINT:              //Pull from our backbuffer, onto the screen
            BitBlt(WindowDC, 0, 0, WindowWidth, WindowHeight, backbuffer, 0, 0,SRCCOPY);
            ValidateRect(WindowHandle,NULL);
            break;
        case WM_DESTROY:
            exit(0);//A messy exit, but easy way to escape game loop
        default://If we didnt process a message, return the default value for it
            return DefWindowProcW(hWnd, Msg, wParam, lParam);
    };
    return 0;
}

//The following 3 methods use mem functions for fast drawing
inline void VertLineDIB(int x, int y, int y2,int thickness, unsigned char color)
{
    int j;
    for (j=y; j<y2; j++)
        memset(&dcbits[x+j*WindowWidth],color,thickness);
}
inline void HorzLineDIB(int x, int y, int x2,int thickness, unsigned char color)
{
    int j;
    for (j=y; j<y+thickness; j++)
        memset(&dcbits[x+j*WindowWidth],color,x2-x);
}
inline void FillRectDIB(int x, int y, int width, int height, unsigned char color)
{
    int j;
    for (j=y; j<y+height; j++)
        //NOTE TO FUTURE: this breaks if j is negative. Apparently it doesn't break if j is too large, though?
        memset(&dcbits[x+j*WindowWidth],color,width);
}

void curses_drawwindow(WINDOW *win)
{
    int i,j,w,drawx,drawy;
    unsigned tmp;
    RECT update = {win->x * fontwidth, -1,
                   (win->x + win->width) * fontwidth, -1};

    for (j=0; j<win->height; j++){
        if (win->line[j].touched)
        {
            update.bottom = (win->y+j+1)*fontheight;
            if (update.top == -1)
            {
                update.top = update.bottom - fontheight;
            }

            win->line[j].touched=false;

            for (i=0,w=0; w<win->width; i++,w++){
                drawx=((win->x+w)*fontwidth);
                drawy=((win->y+j)*fontheight);//-j;
                if (((drawx+fontwidth)<=WindowWidth) && ((drawy+fontheight)<=WindowHeight)){
				const char* utf8str = win->line[j].chars+i;
				int len = ANY_LENGTH;
                tmp = UTF8_getch(&utf8str, &len);
                int FG = win->line[j].FG[w];
                int BG = win->line[j].BG[w];
                FillRectDIB(drawx,drawy,fontwidth,fontheight,BG);

                if ( tmp != UNKNOWN_UNICODE){

                    int color = RGB(windowsPalette[FG].rgbRed,windowsPalette[FG].rgbGreen,windowsPalette[FG].rgbBlue);
                    SetTextColor(backbuffer,color);

					int cw = mk_wcwidth((wchar_t)tmp);
					len = ANY_LENGTH-len;
					if(cw>1)
					{
						FillRectDIB(drawx+fontwidth*(cw-1),drawy,fontwidth,fontheight,BG);
						w+=cw-1;
					}
					if(len>1)
					{
						i+=len-1;
					}
                    if(tmp) ExtTextOutW (backbuffer,drawx,drawy,0,NULL,(WCHAR*)&tmp,1,NULL);
                } else {
                    switch ((unsigned char)win->line[j].chars[i]) {
                    case LINE_OXOX_C://box bottom/top side (horizontal line)
                        HorzLineDIB(drawx,drawy+halfheight,drawx+fontwidth,1,FG);
                        break;
                    case LINE_XOXO_C://box left/right side (vertical line)
                        VertLineDIB(drawx+halfwidth,drawy,drawy+fontheight,2,FG);
                        break;
                    case LINE_OXXO_C://box top left
                        HorzLineDIB(drawx+halfwidth,drawy+halfheight,drawx+fontwidth,1,FG);
                        VertLineDIB(drawx+halfwidth,drawy+halfheight,drawy+fontheight,2,FG);
                        break;
                    case LINE_OOXX_C://box top right
                        HorzLineDIB(drawx,drawy+halfheight,drawx+halfwidth,1,FG);
                        VertLineDIB(drawx+halfwidth,drawy+halfheight,drawy+fontheight,2,FG);
                        break;
                    case LINE_XOOX_C://box bottom right
                        HorzLineDIB(drawx,drawy+halfheight,drawx+halfwidth,1,FG);
                        VertLineDIB(drawx+halfwidth,drawy,drawy+halfheight+1,2,FG);
                        break;
                    case LINE_XXOO_C://box bottom left
                        HorzLineDIB(drawx+halfwidth,drawy+halfheight,drawx+fontwidth,1,FG);
                        VertLineDIB(drawx+halfwidth,drawy,drawy+halfheight+1,2,FG);
                        break;
                    case LINE_XXOX_C://box bottom north T (left, right, up)
                        HorzLineDIB(drawx,drawy+halfheight,drawx+fontwidth,1,FG);
                        VertLineDIB(drawx+halfwidth,drawy,drawy+halfheight,2,FG);
                        break;
                    case LINE_XXXO_C://box bottom east T (up, right, down)
                        VertLineDIB(drawx+halfwidth,drawy,drawy+fontheight,2,FG);
                        HorzLineDIB(drawx+halfwidth,drawy+halfheight,drawx+fontwidth,1,FG);
                        break;
                    case LINE_OXXX_C://box bottom south T (left, right, down)
                        HorzLineDIB(drawx,drawy+halfheight,drawx+fontwidth,1,FG);
                        VertLineDIB(drawx+halfwidth,drawy+halfheight,drawy+fontheight,2,FG);
                        break;
                    case LINE_XXXX_C://box X (left down up right)
                        HorzLineDIB(drawx,drawy+halfheight,drawx+fontwidth,1,FG);
                        VertLineDIB(drawx+halfwidth,drawy,drawy+fontheight,2,FG);
                        break;
                    case LINE_XOXX_C://box bottom east T (left, down, up)
                        VertLineDIB(drawx+halfwidth,drawy,drawy+fontheight,2,FG);
                        HorzLineDIB(drawx,drawy+halfheight,drawx+halfwidth,1,FG);
                        break;
                    default:
                        break;
                    }
                    };//switch (tmp)
                }//(tmp < 0)
            };//for (i=0;i<_windows[w].width;i++)
        }
    };// for (j=0;j<_windows[w].height;j++)
    win->draw=false;                //We drew the window, mark it as so
    if (update.top != -1)
    {
        RedrawWindow(WindowHandle, &update, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
    }
}


//Check for any window messages (keypress, paint, mousemove, etc)
void CheckMessages()
{
    MSG msg;
    while (PeekMessage(&msg, 0 , 0, 0, PM_REMOVE)){
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

//***********************************
//Psuedo-Curses Functions           *
//***********************************

//Basic Init, create the font, backbuffer, etc
WINDOW *curses_init(void)
{
   // _windows = new WINDOW[20];         //initialize all of our variables
    BITMAPINFO bmi;
    lastchar=-1;
    inputdelay=-1;
    std::string typeface;
char * typeface_c;
std::ifstream fin;
fin.open("data\\FONTDATA");
 if (!fin.is_open()){
     MessageBox(WindowHandle, "Failed to open FONTDATA, loading defaults.",
                NULL, 0);
     fontheight=16;
     fontwidth=8;
 } else {
     getline(fin, typeface);
     typeface_c= new char [typeface.size()+1];
     strcpy (typeface_c, typeface.c_str());
     fin >> fontwidth;
     fin >> fontheight;
     if ((fontwidth <= 4) || (fontheight <=4)){
         MessageBox(WindowHandle, "Invalid font size specified!",
                    NULL, 0);
        fontheight=16;
        fontwidth=8;
     }
 }
    halfwidth=fontwidth / 2;
    halfheight=fontheight / 2;
    WindowWidth= (55 + (OPTIONS[OPT_VIEWPORT_X] * 2 + 1)) * fontwidth;
    WindowHeight= (OPTIONS[OPT_VIEWPORT_Y] * 2 + 1) *fontheight;
    WindowX=(GetSystemMetrics(SM_CXSCREEN) / 2)-WindowWidth/2;    //center this
    WindowY=(GetSystemMetrics(SM_CYSCREEN) / 2)-WindowHeight/2;   //sucker
    WinCreate();    //Create the actual window, register it, etc
    CheckMessages();    //Let the message queue handle setting up the window
    WindowDC = GetDC(WindowHandle);
    backbuffer = CreateCompatibleDC(WindowDC);
    ZeroMemory(&bmi, sizeof(BITMAPINFO));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = WindowWidth;
    bmi.bmiHeader.biHeight = -WindowHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount=8;
    bmi.bmiHeader.biCompression = BI_RGB;   //store it in uncompressed bytes
    bmi.bmiHeader.biSizeImage = WindowWidth * WindowHeight * 1;
    bmi.bmiHeader.biClrUsed=16;         //the number of colors in our palette
    bmi.bmiHeader.biClrImportant=16;    //the number of colors in our palette
    backbit = CreateDIBSection(0, &bmi, DIB_RGB_COLORS, (void**)&dcbits, NULL, 0);
    DeleteObject(SelectObject(backbuffer, backbit));//load the buffer into DC

 int nResults = AddFontResourceExA("data\\termfont",FR_PRIVATE,NULL);
   if (nResults>0){
    font = CreateFont(fontheight, fontwidth, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                      ANSI_CHARSET, OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
                      PROOF_QUALITY, FF_MODERN, typeface_c);   //Create our font

  } else {
      MessageBox(WindowHandle, "Failed to load default font, using FixedSys.",
                NULL, 0);
       font = CreateFont(fontheight, fontwidth, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                      ANSI_CHARSET, OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
                      PROOF_QUALITY, FF_MODERN, "FixedSys");   //Create our font
   }
    //FixedSys will be user-changable at some point in time??
    SetBkMode(backbuffer, TRANSPARENT);//Transparent font backgrounds
    SelectObject(backbuffer, font);//Load our font into the DC
//    WindowCount=0;

    delete typeface_c;
    mainwin = newwin((OPTIONS[OPT_VIEWPORT_Y] * 2 + 1),(55 + (OPTIONS[OPT_VIEWPORT_Y] * 2 + 1)),0,0);
    return mainwin;   //create the 'stdscr' window and return its ref
}

//Not terribly sure how this function is suppose to work,
//but jday helped to figure most of it out
int curses_getch(WINDOW* win)
{
 // standards note: getch is sometimes required to call refresh
 // see, e.g., http://linux.die.net/man/3/getch
 // so although it's non-obvious, that refresh() call (and maybe InvalidateRect?) IS supposed to be there
 wrefresh(win);
 InvalidateRect(WindowHandle,NULL,true);
 lastchar=ERR;//ERR=-1
    if (inputdelay < 0)
    {
        do
        {
            CheckMessages();
            if (lastchar!=ERR) break;
            MsgWaitForMultipleObjects(0, NULL, FALSE, 50, QS_ALLEVENTS);//low cpu wait!
        }
        while (lastchar==ERR);
    }
    else if (inputdelay > 0)
    {
        unsigned long starttime=GetTickCount();
        unsigned long endtime;
        do
        {
            CheckMessages();        //MsgWaitForMultipleObjects won't work very good here
            endtime=GetTickCount(); //it responds to mouse movement, and WM_PAINT, not good
            if (lastchar!=ERR) break;
            Sleep(2);
        }
        while (endtime<(starttime+inputdelay));
    }
    else
    {
        CheckMessages();
    };
    Sleep(25);
    return lastchar;
}

//Ends the terminal, destroy everything
int curses_destroy(void)
{
    DeleteObject(font);
    WinDestroy();
    RemoveFontResourceExA("data\\termfont",FR_PRIVATE,NULL);//Unload it
    return 1;
}

inline RGBQUAD BGR(int b, int g, int r)
{
    RGBQUAD result;
    result.rgbBlue=b;    //Blue
    result.rgbGreen=g;    //Green
    result.rgbRed=r;    //Red
    result.rgbReserved=0;//The Alpha, isnt used, so just set it to 0
    return result;
}

int curses_start_color(void)
{
 colorpairs=new pairs[50];
 windowsPalette=new RGBQUAD[16]; //Colors in the struct are BGR!! not RGB!!
 windowsPalette[0]= BGR(0,0,0); // Black
 windowsPalette[1]= BGR(0, 0, 255); // Red
 windowsPalette[2]= BGR(0,110,0); // Green
 windowsPalette[3]= BGR(23,51,92); // Brown???
 windowsPalette[4]= BGR(200, 0, 0); // Blue
 windowsPalette[5]= BGR(98, 58, 139); // Purple
 windowsPalette[6]= BGR(180, 150, 0); // Cyan
 windowsPalette[7]= BGR(150, 150, 150);// Gray
 windowsPalette[8]= BGR(99, 99, 99);// Dark Gray
 windowsPalette[9]= BGR(150, 150, 255); // Light Red/Salmon?
 windowsPalette[10]= BGR(0, 255, 0); // Bright Green
 windowsPalette[11]= BGR(0, 255, 255); // Yellow
 windowsPalette[12]= BGR(255, 100, 100); // Light Blue
 windowsPalette[13]= BGR(240, 0, 255); // Pink
 windowsPalette[14]= BGR(255, 240, 0); // Light Cyan?
 windowsPalette[15]= BGR(255, 255, 255); //White
 return SetDIBColorTable(backbuffer, 0, 16, windowsPalette);
}

void curses_timeout(int t)
{
    inputdelay = t;
}

#endif
