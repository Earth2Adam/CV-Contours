
#define SQR(x) ((x)*(x))	/* macro for square */
#ifndef M_PI			/* in case M_PI not found in math.h */
#define M_PI 3.1415927
#endif
#ifndef M_E
#define M_E 2.718282
#endif

#define MAX_FILENAME_CHARS	320
#define SQR(x) ((x)*(x))
#define MAX_QUEUE 10000	/* max perimeter size (pixels) of border wavefront */
#define MAXCONTOURS 25


char	filename[MAX_FILENAME_CHARS];

HWND	MainWnd;

		// Display flags
int		ShowPixelCoords;

		// Image data
unsigned char *Old_OriginalImage;
unsigned char	*OriginalImage;
unsigned char* normalized_sobel;
double* sobel_image;
unsigned char* smoothed;
int				ROWS,COLS;
int				ContourCount;
int				**ContoursX;
int             **ContoursY;
int				*ContourSizes;
int* contour_data;
int* contour_point_data;


#define TIMER_SECOND	1			/* ID of timer used for animation */

		// Drawing flags
int		TimerRow,TimerCol;
int		ThreadRow,ThreadCol;
int		ThreadRunning;
int		BigDots;
int     GrowColor;
int		PlayMode;
int		mouse_x,mouse_y;
int	    shift_x, shift_y;
int		Step;
int		ShowOriginalImage;
int		intensity, distance;
int		LeftHold;
int     ShiftDown = 0;

		// Function prototypes
LRESULT CALLBACK WndProc(HWND,UINT,WPARAM,LPARAM);
BOOL CALLBACK DlgProc(HWND, UINT, WPARAM, LPARAM);
void PaintImage();
void DrawLineThread(void*);
void LeftClickContourThread(void*);
void RightClickContourThread(void*);
void DrawCircleThread(void*);
void ShiftDragContourThread(void*);


/* Active Contour Declarations */
int WINSIZE = 15;
int CENTER = 1;
double DISWEIGHT = 1.2;
double DEVWEIGHT = 1.2;
double EXTWEIGHT = 1;
int SLOWGROW = 0;
int MAXITERATIONS = 20;



int sobel_x[] = { -1,  0,  1,
				  -2,  0,  2,
				  -1,  0,  1 };
int sobel_y[] = { -1, -2, -1,
				   0,  0,  0,
				   1,  2,  1 };

double distance_between(int x1, int y1, int x2, int y2);
void  normalize_energy(double* energy);
void normalize_sobel_image();
void pad_sobel_image();
void print_energy(double* energy);
unsigned char* ScaleImage(unsigned char* img);
