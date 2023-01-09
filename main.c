
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <sys/timeb.h>
#include <windows.h>
#include <wingdi.h>
#include <winuser.h>
#include <process.h>	/* needed for multithreading */
#include "resource.h"
#include "globals.h"


int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	LPTSTR lpCmdLine, int nCmdShow)

{
	MSG			msg;
	HWND		hWnd;
	WNDCLASS	wc;

	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = (WNDPROC)WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(hInstance, "ID_PLUS_ICON");
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName = "ID_MAIN_MENU";
	wc.lpszClassName = "PLUS";

	if (!RegisterClass(&wc))
		return(FALSE);

	hWnd = CreateWindow("PLUS", "plus program",
		WS_OVERLAPPEDWINDOW | WS_HSCROLL | WS_VSCROLL,
		CW_USEDEFAULT, 0, 400, 400, NULL, NULL, hInstance, NULL);
	if (!hWnd)
		return(FALSE);

	ShowScrollBar(hWnd, SB_BOTH, FALSE);
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);
	MainWnd = hWnd;

	ShowPixelCoords = 0;
	BigDots = 0;
	GrowColor = 0;
	PlayMode = 1;
	Step = 0;
	ShowOriginalImage = 0;
	intensity = 50;
	distance = 500;
	ContourCount = 0;
	LeftHold = 0;



	/* initialize checkmarks */
	HMENU hMenu = GetMenu(MainWnd);
	CheckMenuItem(hMenu, ID_COLOR_RED, MF_CHECKED);
	CheckMenuItem(hMenu, ID_GROWMODE_PLAY, MF_CHECKED);


	strcpy(filename, "");
	OriginalImage = NULL;
	ROWS = COLS = 0;

	InvalidateRect(hWnd, NULL, TRUE);
	UpdateWindow(hWnd);

	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return(msg.wParam);
}




LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg,
	WPARAM wParam, LPARAM lParam)

{
	HMENU				hMenu;
	OPENFILENAME		ofn;
	FILE* fpt;
	HDC					hDC;
	char				header[320], text[320];
	int					BYTES, xPos, yPos, x, y, cur_contour_size;
	int					i, r, c, Gx, Gy, dr, dc;

	switch (uMsg)
	{
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case ID_SHOWPIXELCOORDS:
			ShowPixelCoords = (ShowPixelCoords + 1) % 2;
			PaintImage();
			break;
		case ID_DISPLAY_BIGDOTS:
			BigDots = (BigDots + 1) % 2;
			PaintImage();
			break;
		case ID_COLOR_BLUE:
			GrowColor = 2;
			break;
		case ID_COLOR_GREEN:
			GrowColor = 1;
			break;
		case ID_COLOR_RED:
			GrowColor = 0;
			break;
		case ID_GROWMODE_PLAY:
			PlayMode = 1;
			Step = 1;
			break;
		case ID_GROWMODE_STEP:
			PlayMode = 0;
			break;
		case ID_DISPLAY_CLEARIMAGE:
			ShowOriginalImage = 1;
			Sleep(100);
			ShowOriginalImage = 0;
			PaintImage();
			break;
		case ID_PARAMETERS_ADJUSTPARAMS:
			DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG1), hWnd, DlgProc);
			break;
		case ID_FILE_LOAD:
			if (OriginalImage != NULL)
			{
				free(OriginalImage);
				OriginalImage = NULL;
			}
			memset(&(ofn), 0, sizeof(ofn));
			ofn.lStructSize = sizeof(ofn);
			ofn.lpstrFile = filename;
			filename[0] = 0;
			ofn.nMaxFile = MAX_FILENAME_CHARS;
			ofn.Flags = OFN_EXPLORER | OFN_HIDEREADONLY;
			ofn.lpstrFilter = "PNM files\0*.pnm\0All files\0*.*\0\0";
			if (!(GetOpenFileName(&ofn)) || filename[0] == '\0')
				break;		/* user cancelled load */
			if ((fpt = fopen(filename, "rb")) == NULL)
			{
				MessageBox(NULL, "Unable to open file", filename, MB_OK | MB_APPLMODAL);
				break;
			}
			fscanf(fpt, "%s %d %d %d", header, &COLS, &ROWS, &BYTES);
			if (strcmp(header, "P6") != 0 || BYTES != 255)
			{
				MessageBox(NULL, "Not a PNM (P6) image", filename, MB_OK | MB_APPLMODAL);
				fclose(fpt);
				break;
			}
			Old_OriginalImage = (unsigned char*)calloc(ROWS * COLS * 3, 1);
			header[0] = fgetc(fpt);	/* whitespace character after header */
			fread(Old_OriginalImage, 3, ROWS * COLS, fpt);
			fclose(fpt);

			OriginalImage = (unsigned char*)calloc(ROWS * COLS, 1);


			int i, j = 0;
			for (i = 0; i < ROWS * COLS * 3; i += 3) {
				OriginalImage[j] = (Old_OriginalImage[i] + Old_OriginalImage[i + 1] + Old_OriginalImage[i + 2]) / 3;
				j++;
			}

			// if COLS > 850 or ROWS > 1150 HALVE SIZE
			if (COLS > 850 || ROWS > 1100) {
				OriginalImage = ScaleImage(OriginalImage);
			}


			// intialize contour arrays
			ContoursX = (int**)calloc(MAXCONTOURS, sizeof(int*));
			ContoursY = (int**)calloc(MAXCONTOURS, sizeof(int*));
			ContourSizes = (int*)calloc(MAXCONTOURS, sizeof(int));
			contour_data = (int*)calloc(ROWS * COLS, sizeof(int));
			contour_point_data = (int*)calloc(ROWS * COLS, sizeof(int));

			for (i = 0; i < ROWS * COLS; i++) {
				contour_data[i] = -1;
				contour_point_data[i] = -1;
			}

			for (i = 0; i < MAXCONTOURS; i++) {
				ContoursX[i] = (int*)calloc(ROWS * COLS, sizeof(int));
				ContoursY[i] = (int*)calloc(ROWS * COLS, sizeof(int));
				ContourSizes[i] = 0;
			}

			for (i = 0; i < MAXCONTOURS; i++) {
				for (j = 0; j < ROWS * COLS; j++) {
					ContoursX[i][j] = -1;
					ContoursY[i][j] = -1;
				}
			}

			// Create Sobel Image for Active Contour Algorithms
			sobel_image = (double*)calloc(ROWS * COLS, sizeof(double));
			normalized_sobel = (unsigned char*)calloc(ROWS * COLS, sizeof(unsigned char));

			for (r = 1; r < ROWS - 1; r++) {
				for (c = 1; c < COLS - 1; c++) {
					Gx = 0;
					Gy = 0;
					for (dr = -1; dr <= 1; dr++) {
						for (dc = -1; dc <= 1; dc++) {
							Gx += sobel_x[(CENTER + dr) * 3 + dc + CENTER] * OriginalImage[(r + dr) * COLS + (c + dc)];
							Gy += sobel_y[(CENTER + dr) * 3 + dc + CENTER] * OriginalImage[(r + dr) * COLS + (c + dc)];
						}
					}
					sobel_image[r * COLS + c] = sqrt(SQR(Gx) + SQR(Gy));

				}
			}

			normalize_sobel_image();

			/* smooth
			int c2, r2;
			double left, sum;

			double * intermediate = (double*)calloc(ROWS * COLS, sizeof(double));
			smoothed = (unsigned char*)calloc(ROWS * COLS, sizeof(unsigned char));

			for (r = 0; r < ROWS; r++) {
				for (c = 3; c < COLS - 3; c++) {

					// if first run of the row, calculate the sum
					if (c == 3) {
						sum = 0;
						for (c2 = -3; c2 <= 3; c2++) {

							// save leftmost pixel ** left is an int right now, may need to be char
							if (c2 == -3) {
								left = normalized_sobel[(r)*COLS + (c + c2)];
							}
							sum += normalized_sobel[(r)*COLS + (c + c2)];
						}
					}

					// if not first sum of the row, calculate the sum based on the current sum
					else {
						sum = sum + normalized_sobel[(r)*COLS + (c + 3)] - left;
						left = normalized_sobel[(r)*COLS + (c - 3)];
					}

					intermediate[r * COLS + c] = sum;
				}
			}

			for (c = 0; c < COLS; c++) {
				for (r = 3; r < ROWS - 3; r++) {

					// if first sum of the column, calculate it
					if (r == 3) {
						sum = 0;
						for (r2 = -3; r2 <= 3; r2++) {

							// save leftmost pixel
							if (r2 == -3) {
								left = intermediate[(r + r2) * COLS + (c)];
							}
							sum += intermediate[(r + r2) * COLS + (c)];
						}
					}

					// if not first sum of the column, calculate the sum based on the current sum
					else {
						sum = sum + intermediate[(r + 3) * COLS + (c)] - left;
						left = intermediate[(r - 3) * COLS + (c)];
					}

					smoothed[r * COLS + c] = sum / 49;
				}
			}


			//pad_sobel_image();



			for (i = 0; i < ROWS * COLS; i++) {
				if (normalized_sobel[i] < 50)
					normalized_sobel[i] = 0;
				else {
					normalized_sobel[i] = 255;
				}
			}
			*/


			/* sharpen
			unsigned char pixel_val;
			unsigned char* final_sobel = (unsigned char*)calloc(ROWS * COLS, sizeof(unsigned char));

			for (r = 1; r < ROWS - 1; r++) {
				for (c = 1; c < COLS - 1; c++) {
					pixel_val = normalized_sobel[r * COLS + c];
					sum = 0;
					for (r2 = -1; r2 <= 1; r2++) {
						for (c2 = -1; c2 <= 1; c2++) {
							if (r2 == 0 && c2 == 0) {
								continue;
							}
							sum += normalized_sobel[(r + r2) * COLS + c + c2];
						}
					}
					sum /= 8;
					sum = pixel_val - sum;
					final_sobel[r * COLS + c] = pixel_val + sum;
				}
			}

			*/
			//	OriginalImage = normalized_sobel;


			SetWindowText(hWnd, filename);
			PaintImage();
			break;

		case ID_FILE_QUIT:
			DestroyWindow(hWnd);
			break;
		}
		break;
	case WM_SIZE:		  /* could be used to detect when window size changes */
		PaintImage();
		return(DefWindowProc(hWnd, uMsg, wParam, lParam));
		break;
	case WM_PAINT:
		PaintImage();
		return(DefWindowProc(hWnd, uMsg, wParam, lParam));
		break;
	case WM_LBUTTONDOWN:

		if (ShiftDown) {
			shift_x = LOWORD(lParam);
			shift_y = HIWORD(lParam);
		}
		else if (ContourCount >= MAXCONTOURS) {
			MessageBox(NULL, "Max # of Contours Reached. Please Restart.", "Uh oh!", MB_OK | MB_APPLMODAL);
		}
		else {
			LeftHold = 1;
		}
		return(DefWindowProc(hWnd, uMsg, wParam, lParam));
		break;
	case WM_LBUTTONUP:
		if (ShiftDown) {
			mouse_x = LOWORD(lParam);
			mouse_y = HIWORD(lParam);
			_beginthread(ShiftDragContourThread, 0, MainWnd);
			ShiftDown = 0;
		}
		else if (LeftHold) {
			LeftHold = 0;
			if (ContourSizes[ContourCount] != 0) {
				_beginthread(LeftClickContourThread, 0, MainWnd);
			}
		}
		return(DefWindowProc(hWnd, uMsg, wParam, lParam));
		break;
	case WM_RBUTTONUP:
		if (ContourCount >= MAXCONTOURS) {
			MessageBox(NULL, "Max # of Contours Reached. Please Restart.", "Uh oh!", MB_OK | MB_APPLMODAL);
		}
		else {
			mouse_x = LOWORD(lParam);
			mouse_y = HIWORD(lParam);
			_beginthread(RightClickContourThread, 0, MainWnd);

			//_beginthread(DrawCircleThread, 0, MainWnd);
		}

		return(DefWindowProc(hWnd, uMsg, wParam, lParam));
		break;
	case WM_MOUSEMOVE:
		if (LeftHold && !ShiftDown) {
			mouse_x = LOWORD(lParam);
			mouse_y = HIWORD(lParam);
			if (mouse_x >= 0 && mouse_x < COLS && mouse_y >= 0 && mouse_y < ROWS) {
				hDC = GetDC(MainWnd);
				cur_contour_size = ContourSizes[ContourCount];
				SetPixel(hDC, mouse_x, mouse_y, RGB(255, 0, 0));
				ContoursX[ContourCount][cur_contour_size] = mouse_x;
				ContoursY[ContourCount][cur_contour_size] = mouse_y;
				ContourSizes[ContourCount] += 1;
				ReleaseDC(MainWnd, hDC);
			}
		}
		return(DefWindowProc(hWnd, uMsg, wParam, lParam));
		break;
	case WM_KEYDOWN:
		if (wParam == VK_SHIFT) {
			ShiftDown = 1;
		}

		return(DefWindowProc(hWnd, uMsg, wParam, lParam));
		break;
	case WM_KEYUP:
		if (wParam == VK_SHIFT) {
			ShiftDown = 0;
		}

		return(DefWindowProc(hWnd, uMsg, wParam, lParam));
		break;
	case WM_TIMER:	  /* this event gets triggered every time the timer goes off */
		hDC = GetDC(MainWnd);
		SetPixel(hDC, TimerCol, TimerRow, RGB(0, 0, 255));	/* color the animation pixel blue */
		ReleaseDC(MainWnd, hDC);
		TimerRow++;
		TimerCol += 2;
		break;
	case WM_HSCROLL:	  /* this event could be used to change what part of the image to draw */
		PaintImage();	  /* direct PaintImage calls eliminate flicker; the alternative is InvalidateRect(hWnd,NULL,TRUE); UpdateWindow(hWnd); */
		return(DefWindowProc(hWnd, uMsg, wParam, lParam));
		break;
	case WM_VSCROLL:	  /* this event could be used to change what part of the image to draw */
		PaintImage();
		return(DefWindowProc(hWnd, uMsg, wParam, lParam));
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return(DefWindowProc(hWnd, uMsg, wParam, lParam));
		break;
	}

	hMenu = GetMenu(MainWnd);

	if (GrowColor == 0) {
		CheckMenuItem(hMenu, ID_COLOR_RED, MF_CHECKED);
		CheckMenuItem(hMenu, ID_COLOR_GREEN, MF_UNCHECKED);
		CheckMenuItem(hMenu, ID_COLOR_BLUE, MF_UNCHECKED);
	}
	else if (GrowColor == 1) {
		CheckMenuItem(hMenu, ID_COLOR_RED, MF_UNCHECKED);
		CheckMenuItem(hMenu, ID_COLOR_GREEN, MF_CHECKED);
		CheckMenuItem(hMenu, ID_COLOR_BLUE, MF_UNCHECKED);
	}
	else {
		CheckMenuItem(hMenu, ID_COLOR_RED, MF_UNCHECKED);
		CheckMenuItem(hMenu, ID_COLOR_GREEN, MF_UNCHECKED);
		CheckMenuItem(hMenu, ID_COLOR_BLUE, MF_CHECKED);
	}

	if (PlayMode == 1) {
		CheckMenuItem(hMenu, ID_GROWMODE_STEP, MF_UNCHECKED);
		CheckMenuItem(hMenu, ID_GROWMODE_PLAY, MF_CHECKED);
	}
	else {
		CheckMenuItem(hMenu, ID_GROWMODE_STEP, MF_CHECKED);
		CheckMenuItem(hMenu, ID_GROWMODE_PLAY, MF_UNCHECKED);
	}

	if (ShowPixelCoords == 1)
		CheckMenuItem(hMenu, ID_SHOWPIXELCOORDS, MF_CHECKED);	/* you can also call EnableMenuItem() to grey(disable) an option */
	else
		CheckMenuItem(hMenu, ID_SHOWPIXELCOORDS, MF_UNCHECKED);
	if (BigDots == 1)
		CheckMenuItem(hMenu, ID_DISPLAY_BIGDOTS, MF_CHECKED);	/* you can also call EnableMenuItem() to grey(disable) an option */
	else
		CheckMenuItem(hMenu, ID_DISPLAY_BIGDOTS, MF_UNCHECKED);

	DrawMenuBar(hWnd);

	return(0L);
}


void PaintImage()

{
	PAINTSTRUCT			Painter;
	HDC					hDC;
	BITMAPINFOHEADER	bm_info_header;
	BITMAPINFO* bm_info;
	int					i, r, c, DISPLAY_ROWS, DISPLAY_COLS;
	unsigned char* DisplayImage;

	if (OriginalImage == NULL)
		return;		/* no image to draw */

			  /* Windows pads to 4-byte boundaries.  We have to round the size up to 4 in each dimension, filling with black. */
	DISPLAY_ROWS = ROWS;
	DISPLAY_COLS = COLS;
	if (DISPLAY_ROWS % 4 != 0)
		DISPLAY_ROWS = (DISPLAY_ROWS / 4 + 1) * 4;
	if (DISPLAY_COLS % 4 != 0)
		DISPLAY_COLS = (DISPLAY_COLS / 4 + 1) * 4;
	DisplayImage = (unsigned char*)calloc(DISPLAY_ROWS * DISPLAY_COLS, 1);
	for (r = 0; r < ROWS; r++)
		for (c = 0; c < COLS; c++)
			DisplayImage[r * DISPLAY_COLS + c] = OriginalImage[r * COLS + c];

	BeginPaint(MainWnd, &Painter);
	hDC = GetDC(MainWnd);
	bm_info_header.biSize = sizeof(BITMAPINFOHEADER);
	bm_info_header.biWidth = DISPLAY_COLS;
	bm_info_header.biHeight = -DISPLAY_ROWS;
	bm_info_header.biPlanes = 1;
	bm_info_header.biBitCount = 8;
	bm_info_header.biCompression = BI_RGB;
	bm_info_header.biSizeImage = 0;
	bm_info_header.biXPelsPerMeter = 0;
	bm_info_header.biYPelsPerMeter = 0;
	bm_info_header.biClrUsed = 256;
	bm_info_header.biClrImportant = 256;
	bm_info = (BITMAPINFO*)calloc(1, sizeof(BITMAPINFO) + 256 * sizeof(RGBQUAD));
	bm_info->bmiHeader = bm_info_header;
	for (i = 0; i < 256; i++)
	{
		bm_info->bmiColors[i].rgbBlue = bm_info->bmiColors[i].rgbGreen = bm_info->bmiColors[i].rgbRed = i;
		bm_info->bmiColors[i].rgbReserved = 0;
	}

	SetDIBitsToDevice(hDC, 0, 0, DISPLAY_COLS, DISPLAY_ROWS, 0, 0,
		0, /* first scan line */
		DISPLAY_ROWS, /* number of scan lines */
		DisplayImage, bm_info, DIB_RGB_COLORS);
	ReleaseDC(MainWnd, hDC);
	EndPaint(MainWnd, &Painter);

	free(DisplayImage);
	free(bm_info);
}



void DrawLineThread(HWND AnimationWindowHandle)

{
	HDC		hDC;
	int		width;
	int		x, y, cx, cy;

	cx = mouse_x;
	cy = mouse_y;
	width = 3;
	int FirstPixel = 1;

	while (width < 62)
	{
		if (ShowOriginalImage) {
			ShowOriginalImage = 0;
			break;
		}


		x = cx;
		y = cy;
		hDC = GetDC(MainWnd);

		Step = 0;

		if (PlayMode == 1) {

			if (GrowColor == 0)
				SetPixel(hDC, x + width, y, RGB(255, 0, 0));	/* color the line red */
			else if (GrowColor == 1)
				SetPixel(hDC, x + width, y, RGB(0, 255, 0));	/* color the line green */
			else
				SetPixel(hDC, x + width, y, RGB(0, 0, 255));	/* color the line blue */
		}
		else {
			if (FirstPixel) {
				FirstPixel = 0;
				if (GrowColor == 0)
					SetPixel(hDC, x + width, y, RGB(255, 0, 0));	/* color the line red */
				else if (GrowColor == 1)
					SetPixel(hDC, x + width, y, RGB(0, 255, 0));	/* color the line green */
				else
					SetPixel(hDC, x + width, y, RGB(0, 0, 255));	/* color the line blue */
			}
			else
				while (1) {
					if (ShowOriginalImage) {
						break;
					}
					if (Step) {
						if (GrowColor == 0)
							SetPixel(hDC, x + width, y, RGB(255, 0, 0));	/* color the line red */
						else if (GrowColor == 1)
							SetPixel(hDC, x + width, y, RGB(0, 255, 0));	/* color the line green */
						else
							SetPixel(hDC, x + width, y, RGB(0, 0, 255));	/* color the line blue */

						break;
					}
				}
		}


		ReleaseDC(MainWnd, hDC);
		width += 1;
		Sleep(1);		/* pause 1 ms */
	}
}




void LeftClickContourThread(HWND AnimationWindowHandle) {
	int CurrentContour = ContourCount;
	int ContourSize = ContourSizes[CurrentContour];
	int CONTOURPOINTS = (int)ceil((double)ContourSize / 5.0);
	ContourSizes[CurrentContour] = CONTOURPOINTS;

	int* ContX = (int*)calloc(CONTOURPOINTS, sizeof(int));
	int* ContY = (int*)calloc(CONTOURPOINTS, sizeof(int));
	int* NextX = (int*)calloc(CONTOURPOINTS, sizeof(int));
	int* NextY = (int*)calloc(CONTOURPOINTS, sizeof(int));

	ContourCount++;

	int i, xPos, yPos, dx, dy, NewIndex, r, c;
	unsigned char og_color;
	HDC		hDC;

	hDC = GetDC(MainWnd);

	// keep every 5th point
	NewIndex = 0;
	for (i = 0; i < ContourSize; i++) {

		xPos = ContoursX[CurrentContour][i];
		yPos = ContoursY[CurrentContour][i];

		if (i % 5 == 0) {
			for (dx = -2; dx <= 2; dx++) {
				for (dy = -2; dy <= 2; dy++) {
					if (CurrentContour % 4 == 0) {
						SetPixel(hDC, xPos + dx, yPos + dy, RGB(0, 0, 255));
					}
					else if (CurrentContour % 4 == 1) {
						SetPixel(hDC, xPos + dx, yPos + dy, RGB(0, 255, 0));
					}
					else if (CurrentContour % 4 == 2) {
						SetPixel(hDC, xPos + dx, yPos + dy, RGB(255, 0, 0));
					}
					else if (CurrentContour % 4 == 3) {
						SetPixel(hDC, xPos + dx, yPos + dy, RGB(255, 0, 255));
					}
				}
			}
			ContX[NewIndex] = xPos;
			ContY[NewIndex] = yPos;
			NewIndex++;
		}
		else {
			og_color = OriginalImage[yPos * COLS + xPos];
			SetPixel(hDC, xPos, yPos, RGB(og_color, og_color, og_color));
		}
	}


	free(ContoursX[CurrentContour]);
	free(ContoursY[CurrentContour]);
	ContoursX[CurrentContour] = ContX;
	ContoursY[CurrentContour] = ContY;

	Sleep(1000);
	for (i = 0; i < CONTOURPOINTS; i++) {

		xPos = ContX[i];
		yPos = ContY[i];

		for (dx = -2; dx <= 2; dx++) {
			for (dy = -2; dy <= 2; dy++) {
				og_color = OriginalImage[(yPos + dy) * COLS + xPos + dx];
				SetPixel(hDC, xPos + dx, yPos + dy, RGB(og_color, og_color, og_color));
			}
		}
	}



	// all declarations needed for the algorithm
	double* distance_energy, * deviation_energy, * external_energy, * final_energy;

	distance_energy = (double*)calloc(SQR(WINSIZE), sizeof(double));
	deviation_energy = (double*)calloc(SQR(WINSIZE), sizeof(double));
	external_energy = (double*)calloc(SQR(WINSIZE), sizeof(double));
	final_energy = (double*)calloc(SQR(WINSIZE), sizeof(double));

	int contour = 0, next_ct, win_index, dr, dc;
	int iterations = 0;
	double min;
	double avg_distance, cur_distance;

	/* Run the active contour algorithm, shrinking the contour to its final state */

	while (1) {

		/* Pre-Processiong: Calculate Average Distance Between all Contour Points
			if new cycle has begun.  */
		if (contour == 0) {
			avg_distance = 0;

			for (i = 0; i < CONTOURPOINTS; i++) {
				next_ct = (i + 1) % CONTOURPOINTS;
				avg_distance += distance_between(ContX[i], ContY[i], ContX[next_ct], ContY[next_ct]);
			}
			avg_distance /= CONTOURPOINTS;
		}



		/* A. Calculate Internal Distance Energy*/

		win_index = 0;
		next_ct = (contour + 1) % CONTOURPOINTS;
		for (r = -1 * WINSIZE / 2; r <= WINSIZE / 2; r++) {
			for (c = -1 * WINSIZE / 2; c <= WINSIZE / 2; c++) {
				distance_energy[win_index] = SQR(ContX[contour] + c - ContX[next_ct]) + SQR(ContY[contour] + r - ContY[next_ct]);
				win_index++;
			}
		}


		/* B. Calculate Internal Deviant Energy */

		win_index = 0;
		next_ct = (contour + 1) % CONTOURPOINTS;
		for (r = -1 * WINSIZE / 2; r <= WINSIZE / 2; r++) {
			for (c = -1 * WINSIZE / 2; c <= WINSIZE / 2; c++) {
				cur_distance = distance_between(ContX[contour] + c, ContY[contour] + r, ContX[next_ct], ContY[next_ct]);
				deviation_energy[win_index] = SQR(cur_distance - avg_distance);
				win_index++;
			}
		}



		/* C. Calculate External Energy */

		win_index = 0;
		for (r = -1 * WINSIZE / 2; r <= WINSIZE / 2; r++) {
			for (c = -1 * WINSIZE / 2; c <= WINSIZE / 2; c++) {
				if (r + ContY[contour] >= ROWS || r + ContY[contour] < 0 || c + ContX[contour] >= COLS || c + ContX[contour] < 0) {
					external_energy[win_index] = 0;
				}
				else {
					external_energy[win_index] = -1 * normalized_sobel[(r + ContY[contour]) * COLS + (c + ContX[contour])];
				}
				win_index++;
			}
		}


		/* D. Normalize All Energy Matricies */

		normalize_energy(distance_energy);
		normalize_energy(deviation_energy);
		normalize_energy(external_energy);


		/* E. Calculate Final Energy */
		for (i = 0; i < WINSIZE * WINSIZE; i++) {
			final_energy[i] = DISWEIGHT * distance_energy[i] + DEVWEIGHT * deviation_energy[i] + EXTWEIGHT * external_energy[i];
		}


		/* F. Store Next Location for Current Contour Point */

		min = 10000;
		for (dr = -WINSIZE / 2; dr <= WINSIZE / 2; dr++) {
			for (dc = -WINSIZE / 2; dc <= WINSIZE / 2; dc++) {
				if (final_energy[(dr + (WINSIZE / 2)) * WINSIZE + (dc + (WINSIZE / 2))] < min) {
					NextX[contour] = ContX[contour] + dc;
					NextY[contour] = ContY[contour] + dr;
					min = final_energy[(dr + (WINSIZE / 2)) * WINSIZE + (dc + (WINSIZE / 2))];
				}
			}
		}

		/* G. Final Loop Checks
			-- Was the point unmoved?
			-- Have all new coordinates been calculated?
		*/


		/* move contour points if the current cycle
		  around the contour is complete */
		contour++;
		if (contour == CONTOURPOINTS) {
			contour = 0;
			iterations++;

			// reset color before moving contour
			for (i = 0; i < CONTOURPOINTS; i++) {

				xPos = ContX[i];
				yPos = ContY[i];

				for (dx = -2; dx <= 2; dx++) {
					for (dy = -2; dy <= 2; dy++) {
						if (yPos + dy < 0 || yPos + dy >= ROWS || xPos + dx < 0 || xPos + dx >= COLS) {
							og_color = 255;
						}
						else {
							og_color = OriginalImage[(yPos + dy) * COLS + xPos + dx];
						}
						SetPixel(hDC, xPos + dx, yPos + dy, RGB(og_color, og_color, og_color));
						contour_data[(yPos + dy) * COLS + xPos + dx] = -1;
						contour_point_data[(yPos + dy) * COLS + xPos + dx] = -1;

					}
				}
			}

			for (i = 0; i < CONTOURPOINTS; i++) {
				ContX[i] = NextX[i];
				ContY[i] = NextY[i];
			}

			/* Draw On the New Contour Points */
			for (i = 0; i < CONTOURPOINTS; i++) {

				xPos = ContX[i];
				yPos = ContY[i];

				for (dx = -2; dx <= 2; dx++) {
					for (dy = -2; dy <= 2; dy++) {
						if (CurrentContour % 4 == 0) {
							SetPixel(hDC, xPos + dx, yPos + dy, RGB(0, 0, 255));
						}
						else if (CurrentContour % 4 == 1) {
							SetPixel(hDC, xPos + dx, yPos + dy, RGB(0, 255, 0));
						}
						else if (CurrentContour % 4 == 2) {
							SetPixel(hDC, xPos + dx, yPos + dy, RGB(255, 0, 0));
						}
						else if (CurrentContour % 4 == 3) {
							SetPixel(hDC, xPos + dx, yPos + dy, RGB(255, 0, 255));
						}
						contour_data[(yPos + dy) * COLS + xPos + dx] = CurrentContour;
						contour_point_data[(yPos + dy) * COLS + xPos + dx] = i;
					}
				}
			}
			if (SLOWGROW) {
				Sleep(250);
			}
		}

		/* if iterations exceeds max, break */
		if (iterations == MAXITERATIONS) {
			break;
		}
	}



	/* Draw On the New Contour Points (Or additionally, move this inside the loop to see progression
	for (i = 0; i < CONTOURPOINTS; i++) {

		xPos = ContX[i];
		yPos = ContY[i];

		for (dx = -2; dx <= 2; dx++) {
			for (dy = -2; dy <= 2; dy++) {
				SetPixel(hDC, xPos + dx, yPos + dy, RGB(255, 0, 0));
			}
		}
	}
	*/

	ReleaseDC(MainWnd, hDC);


}


void RightClickContourThread(HWND AnimationWindowHandle) {

	char text[100];
	HDC		hDC;
	hDC = GetDC(MainWnd);
	FILE* fpt;
	int minima_found, xPos, yPos, minx, miny, r, c, dx, dy, i, j, radius = 10; // circle of radius 10
	unsigned char og_color;

	int xc = mouse_x;
	int yc = mouse_y;
	int CONTOURPOINTS = 0;
	int CurrentContour = ContourCount;
	int* ContX = ContoursX[CurrentContour];
	int* ContY = ContoursY[CurrentContour];
	ContourCount++;

	int x = 0, y = radius;
	int d = 3 - 2 * radius;
	int q1x[8], q2x[8], q3x[8], q4x[8], q5x[8], q6x[8], q7x[8], q8x[8];
	int q1y[8], q2y[8], q3y[8], q4y[8], q5y[8], q6y[8], q7y[8], q8y[8];
	i = 0;

	q1x[i] = xc + x;
	q1y[i] = yc - y;
	q2x[i] = xc + y;
	q2y[i] = yc - x;
	q3x[i] = xc + y;
	q3y[i] = yc + x;
	q4x[i] = xc + x;
	q4y[i] = yc + y;
	q5x[i] = xc - x;
	q5y[i] = yc + y;
	q6x[i] = xc - y;
	q6y[i] = yc + x;
	q7x[i] = xc - y;
	q7y[i] = yc - x;
	q8x[i] = xc - x;
	q8y[i] = yc - y;
	CONTOURPOINTS += 8;

	while (y >= x)
	{
		// for each pixel we will
		// draw all eight pixels

		x++;
		i++;

		// check for decision parameter
		// and correspondingly
		// update d, x, y
		if (d > 0)
		{
			y--;
			d = d + 4 * (x - y) + 10;
		}
		else
			d = d + 4 * x + 6;

		q1x[i] = xc + x;
		q1y[i] = yc - y;
		q2x[i] = xc + y;
		q2y[i] = yc - x;
		q3x[i] = xc + y;
		q3y[i] = yc + x;
		q4x[i] = xc + x;
		q4y[i] = yc + y;
		q5x[i] = xc - x;
		q5y[i] = yc + y;
		q6x[i] = xc - y;
		q6y[i] = yc + x;
		q7x[i] = xc - y;
		q7y[i] = yc - x;
		q8x[i] = xc - x;
		q8y[i] = yc - y;
		CONTOURPOINTS += 8;
	}

	int xlist[63];
	int ylist[63];

	for (i = 0; i < 63; i++) {
		if (i < 8) {
			xlist[i] = q1x[i];
			ylist[i] = q1y[i];
		}
		else if (i < 16) {
			xlist[i] = q2x[7 - (i % 8)];
			ylist[i] = q2y[7 - (i % 8)];
		}
		else if (i < 24) {
			xlist[i] = q3x[i % 8];
			ylist[i] = q3y[i % 8];
		}
		else if (i < 32) {
			xlist[i] = q4x[7 - (i % 8)];
			ylist[i] = q4y[7 - (i % 8)];
		}
		else if (i < 40) {
			xlist[i] = q5x[i % 8];
			ylist[i] = q5y[i % 8];
		}
		else if (i < 48) {
			xlist[i] = q6x[7 - (i % 8)];
			ylist[i] = q6y[7 - (i % 8)];
		}
		else if (i < 56) {
			xlist[i] = q7x[i % 8];
			ylist[i] = q7y[i % 8];
		}
		else if (i < 63) {
			xlist[i] = q8x[7 - (i % 8)];
			ylist[i] = q8y[7 - (i % 8)];
		}
	}

	// Keep Every Third Point 
	for (i = 0; i < CONTOURPOINTS / 3; i++) {
		ContX[i] = xlist[i * 3];
		ContY[i] = ylist[i * 3];
		xPos = ContX[i];
		yPos = ContY[i];
		for (dx = -2; dx <= 2; dx++) {
			for (dy = -2; dy <= 2; dy++) {
				SetPixel(hDC, xPos + dx, yPos + dy, RGB(255, 0, 0));

			}
		}
	}


	/* test, keep all contour points
	CONTOURPOINTS--;
	for (i = 0; i < CONTOURPOINTS; i++) {
		ContX[i] = xlist[i];
		ContY[i] = ylist[i];
		for (dx = -2; dx <= 2; dx++) {
			for (dy = -2; dy <= 2; dy++) {
				SetPixel(hDC, ContX[i] + dx, ContY[i] + dy, RGB(255, 0, 0));
			}
		}
	}
	*/

	Sleep(1000);
	CONTOURPOINTS /= 3;
	ContourSizes[CurrentContour] = CONTOURPOINTS;

	// reset color before moving contour
	for (i = 0; i < CONTOURPOINTS; i++) {

		xPos = ContX[i];
		yPos = ContY[i];

		for (dx = -2; dx <= 2; dx++) {
			for (dy = -2; dy <= 2; dy++) {
				og_color = OriginalImage[(yPos + dy) * COLS + xPos + dx];
				SetPixel(hDC, xPos + dx, yPos + dy, RGB(og_color, og_color, og_color));
			}
		}
	}

	// all declarations needed for the algorithm
	double* distance_energy, * deviation_energy, * external_energy, * final_energy;

	distance_energy = (double*)calloc(SQR(WINSIZE), sizeof(double));
	deviation_energy = (double*)calloc(SQR(WINSIZE), sizeof(double));
	external_energy = (double*)calloc(SQR(WINSIZE), sizeof(double));
	final_energy = (double*)calloc(SQR(WINSIZE), sizeof(double));
	int* NextX = (int*)calloc(CONTOURPOINTS, sizeof(int));
	int* NextY = (int*)calloc(CONTOURPOINTS, sizeof(int));

	int contour = 0, next_ct, prev_ct, win_index, dr, dc;
	int iterations = 0;
	double min;
	double avg_distance, cur_distance, dist_prev, dist_next;

	/* Run the active contour algorithm, growing the contour to its final state */

	while (1) {

		/* Pre-Processiong: Calculate Average Distance Between all Contour Points
			if new cycle has begun.  */
		if (contour == 0) {
			avg_distance = 0;

			for (i = 0; i < CONTOURPOINTS; i++) {

				next_ct = (i + 1) % CONTOURPOINTS;
				avg_distance += distance_between(ContX[i], ContY[i], ContX[next_ct], ContY[next_ct]);

			}
			avg_distance /= CONTOURPOINTS;

		}


		/* A. Calculate Internal Distance Energy*/

		win_index = 0;
		next_ct = (contour + 1) % CONTOURPOINTS;
		prev_ct = (contour - 1) % CONTOURPOINTS;

		for (r = -1 * WINSIZE / 2; r <= WINSIZE / 2; r++) {
			for (c = -1 * WINSIZE / 2; c <= WINSIZE / 2; c++) {
				distance_energy[win_index] = -1 * (SQR(ContX[contour] + c - xc) + SQR(ContY[contour] + r - yc));
				win_index++;
			}
		}


		/* B. Calculate Internal Deviant Energy */

		win_index = 0;
		for (r = -1 * WINSIZE / 2; r <= WINSIZE / 2; r++) {
			for (c = -1 * WINSIZE / 2; c <= WINSIZE / 2; c++) {
				cur_distance = distance_between(ContX[contour] + c, ContY[contour] + r, ContX[next_ct], ContY[next_ct]);
				deviation_energy[win_index] = SQR(cur_distance - avg_distance);
				win_index++;
			}
		}


		/* C. Calculate External Energy TODO: add handle for out of bounds access? */

		win_index = 0;
		for (r = -1 * WINSIZE / 2; r <= WINSIZE / 2; r++) {
			for (c = -1 * WINSIZE / 2; c <= WINSIZE / 2; c++) {
				external_energy[win_index] = -1 * normalized_sobel[(r + ContY[contour]) * COLS + (c + ContX[contour])];
				win_index++;
			}
		}

		/* D. Normalize All Energy Matricies */

		normalize_energy(distance_energy);
		normalize_energy(deviation_energy);
		normalize_energy(external_energy);

		/* E. Calculate Final Energy */
		for (i = 0; i < WINSIZE * WINSIZE; i++) {
			if (iterations < 5) {
				final_energy[i] = DISWEIGHT * distance_energy[i] + DEVWEIGHT * deviation_energy[i] + 0 * EXTWEIGHT * external_energy[i];
			}
			else {
				final_energy[i] = DISWEIGHT * distance_energy[i] + DEVWEIGHT * deviation_energy[i] + 3 * 0.75 * EXTWEIGHT * external_energy[i];
			}
		}

		/* F. Store Next Location for Current Contour Point */

		min = 10000;
		minima_found = 0;
		minx = 0;
		miny = 0;
		for (dr = -WINSIZE / 2; dr <= WINSIZE / 2; dr++) {
			for (dc = -WINSIZE / 2; dc <= WINSIZE / 2; dc++) {

				if (final_energy[(dr + (WINSIZE / 2)) * WINSIZE + (dc + (WINSIZE / 2))] == min) {
					minima_found++;
					minx += ContX[contour] + dc;
					miny += ContY[contour] + dr;
				}
				else if (final_energy[(dr + (WINSIZE / 2)) * WINSIZE + (dc + (WINSIZE / 2))] < min) {
					minx = ContX[contour] + dc;
					miny = ContY[contour] + dr;
					min = final_energy[(dr + (WINSIZE / 2)) * WINSIZE + (dc + (WINSIZE / 2))];
					minima_found = 1;
				}
			}
		}

		NextX[contour] = minx / minima_found;
		NextY[contour] = miny / minima_found;


		/* G. Final Loop Checks
			-- Was the point unmoved?
			-- Have all new coordinates been calculated?
		*/

		/* move contour points if the current cycle
			around the contour is complete */
		contour++;
		if (contour == CONTOURPOINTS) {
			contour = 0;
			iterations++;

			// reset color before moving contour
			for (i = 0; i < CONTOURPOINTS; i++) {

				xPos = ContX[i];
				yPos = ContY[i];

				for (dx = -2; dx <= 2; dx++) {
					for (dy = -2; dy <= 2; dy++) {
						if (yPos + dy < 0 || yPos + dy >= ROWS || xPos + dx < 0 || xPos + dx >= COLS) {
							og_color = 255;
						}
						else {
							og_color = OriginalImage[(yPos + dy) * COLS + xPos + dx];
						}
						SetPixel(hDC, xPos + dx, yPos + dy, RGB(og_color, og_color, og_color));
						contour_data[(yPos + dy) * COLS + xPos + dx] = -1;
						contour_point_data[(yPos + dy) * COLS + xPos + dx] = -1;

					}
				}
			}

			for (i = 0; i < CONTOURPOINTS; i++) {
				ContX[i] = NextX[i];
				ContY[i] = NextY[i];
			}

			/* Draw On the New Contour Points */
			for (i = 0; i < CONTOURPOINTS; i++) {

				xPos = ContX[i];
				yPos = ContY[i];

				for (dx = -2; dx <= 2; dx++) {
					for (dy = -2; dy <= 2; dy++) {
						SetPixel(hDC, xPos + dx, yPos + dy, RGB(255, 0, 0));
						contour_data[(yPos + dy) * COLS + xPos + dx] = CurrentContour;
						contour_point_data[(yPos + dy) * COLS + xPos + dx] = i;
					}
				}
			}
			if (SLOWGROW) {
				Sleep(250);
			}
		}


		/* if iterations exceeds max, break */
		if (iterations == MAXITERATIONS) {
			break;
		}

	}


	/* Draw On the New Contour Points (Or additionally, move this inside the loop to see progression
	for (i = 0; i < CONTOURPOINTS; i++) {

		xPos = ContX[i];
		yPos = ContY[i];

		for (dx = -2; dx <= 2; dx++) {
			for (dy = -2; dy <= 2; dy++) {
				SetPixel(hDC, xPos + dx, yPos + dy, RGB(255, 0, 0));
			}
		}
		Sleep(2000);
	} */

	ReleaseDC(MainWnd, hDC);
}


void ShiftDragContourThread(HWND AnimationWindowHandle) {
	HDC		hDC;
	hDC = GetDC(MainWnd);

	char text[100];
	int dx, dy;
	int start_x = shift_x, start_y = shift_y;
	int end_x = mouse_x, end_y = mouse_y;
	int contour_num = contour_data[start_y * COLS + start_x];
	int contour_point_num = contour_point_data[start_y * COLS + start_x];
	int cont_x, cont_y, contour_size, nxt_cont_x, nxt_cont_y, prev_cont_x, prev_cont_y;
	int SHIFTWINSIZE = 7;
	unsigned char og_color;


	if (contour_num == -1 || contour_point_num == -1) {
		MessageBox(NULL, "Something was off.", "Uh oh!", MB_OK | MB_APPLMODAL);
		return;
	}
	else {
		sprintf(text, "Contour: %d  Point %d", contour_num, contour_point_num);
		//	MessageBox(NULL, text, "Well Done.", MB_OK | MB_APPLMODAL);
	}

	contour_size = ContourSizes[contour_num];
	cont_x = ContoursX[contour_num][contour_point_num];
	cont_y = ContoursY[contour_num][contour_point_num];
	nxt_cont_x = ContoursX[contour_num][(contour_point_num + 1) % contour_size];
	nxt_cont_y = ContoursY[contour_num][(contour_point_num + 1) % contour_size];
	prev_cont_x = ContoursX[contour_num][(contour_point_num - 1) % contour_size];
	prev_cont_y = ContoursY[contour_num][(contour_point_num - 1) % contour_size];

	for (dy = -2; dy <= 2; dy++) {
		for (dx = -2; dx <= 2; dx++) {

			og_color = OriginalImage[(cont_y + dy) * COLS + cont_x + dx];
			SetPixel(hDC, cont_x + dx, cont_y + dy, RGB(og_color, og_color, og_color));
			contour_data[(cont_y + dy) * COLS + cont_x + dx] = -1;
			contour_point_data[(cont_y + dy) * COLS + cont_x + dx] = -1;
			SetPixel(hDC, end_x + dx, end_y + dy, RGB(255, 0, 0));
			contour_data[(end_y + dy) * COLS + end_x + dx] = contour_num;
			contour_point_data[(end_y + dy) * COLS + end_x + dx] = contour_point_num;
		}
	}

	// all declarations needed for the algorithm
	double* p_distance_energy, * p_external_energy, * p_final_energy;
	double* n_distance_energy, * n_external_energy, * n_final_energy;

	p_distance_energy = (double*)calloc(SQR(WINSIZE), sizeof(double));
	n_distance_energy = (double*)calloc(SQR(WINSIZE), sizeof(double));
	p_external_energy = (double*)calloc(SQR(WINSIZE), sizeof(double));
	n_external_energy = (double*)calloc(SQR(WINSIZE), sizeof(double));
	p_final_energy = (double*)calloc(SQR(WINSIZE), sizeof(double));
	n_final_energy = (double*)calloc(SQR(WINSIZE), sizeof(double));

	int contour = 0, next_ct, prev_ct, win_index, dr, dc, i, r, c;
	int iterations = 0;
	double min;

	/* Run the active contour algorithm, growing the contour to its final state */
	while (1) {

		Sleep(500);
		/* A. Calculate Internal Distance Energy*/

		win_index = 0;
		for (r = -1 * SHIFTWINSIZE / 2; r <= SHIFTWINSIZE / 2; r++) {
			for (c = -1 * SHIFTWINSIZE / 2; c <= SHIFTWINSIZE / 2; c++) {
				p_distance_energy[win_index] = 1 * (SQR(prev_cont_x + c - end_x) + SQR(prev_cont_y + r - end_y));
				n_distance_energy[win_index] = 1 * (SQR(nxt_cont_x + c - end_x) + SQR(nxt_cont_y + r - end_y));
				win_index++;
			}
		}


		/* C. Calculate External Energy TODO: add handle for out of bounds access? */

		win_index = 0;
		for (r = -1 * SHIFTWINSIZE / 2; r <= SHIFTWINSIZE / 2; r++) {
			for (c = -1 * SHIFTWINSIZE / 2; c <= SHIFTWINSIZE / 2; c++) {
				p_external_energy[win_index] = -1 * normalized_sobel[(r + prev_cont_y) * COLS + (c + prev_cont_x)];
				n_external_energy[win_index] = -1 * normalized_sobel[(r + nxt_cont_y) * COLS + (c + nxt_cont_x)];
				win_index++;
			}
		}

		/* D. Normalize All Energy Matricies */

		double max = -10000;
		double min = INT_MAX;
		int i, r, c;

		for (r = 0; r < SHIFTWINSIZE; r++) {
			for (c = 0; c < SHIFTWINSIZE; c++) {
				if (p_distance_energy[r * SHIFTWINSIZE + c] > max)
					max = p_distance_energy[r * SHIFTWINSIZE + c];
				else if (p_distance_energy[r * SHIFTWINSIZE + c] < min)
					min = p_distance_energy[r * SHIFTWINSIZE + c];
			}
		}

		for (i = 0; i < SQR(SHIFTWINSIZE); i++) {
			p_distance_energy[i] = (p_distance_energy[i] - min) / (max - min);
		}

		max = -10000;
		min = INT_MAX;
		for (r = 0; r < SHIFTWINSIZE; r++) {
			for (c = 0; c < SHIFTWINSIZE; c++) {
				if (p_external_energy[r * SHIFTWINSIZE + c] > max)
					max = p_external_energy[r * SHIFTWINSIZE + c];
				else if (p_external_energy[r * SHIFTWINSIZE + c] < min)
					min = p_external_energy[r * SHIFTWINSIZE + c];
			}
		}
		for (i = 0; i < SQR(SHIFTWINSIZE); i++) {
			p_external_energy[i] = (p_external_energy[i] - min) / (max - min);
		}


		max = -10000;
		min = INT_MAX;
		for (r = 0; r < SHIFTWINSIZE; r++) {
			for (c = 0; c < SHIFTWINSIZE; c++) {
				if (n_external_energy[r * SHIFTWINSIZE + c] > max)
					max = n_external_energy[r * SHIFTWINSIZE + c];
				else if (n_external_energy[r * SHIFTWINSIZE + c] < min)
					min = n_external_energy[r * SHIFTWINSIZE + c];
			}
		}
		for (i = 0; i < SQR(SHIFTWINSIZE); i++) {
			n_external_energy[i] = (n_external_energy[i] - min) / (max - min);
		}

		max = -10000;
		min = INT_MAX;
		for (r = 0; r < SHIFTWINSIZE; r++) {
			for (c = 0; c < SHIFTWINSIZE; c++) {
				if (n_distance_energy[r * SHIFTWINSIZE + c] > max)
					max = n_distance_energy[r * SHIFTWINSIZE + c];
				else if (n_distance_energy[r * SHIFTWINSIZE + c] < min)
					min = n_distance_energy[r * SHIFTWINSIZE + c];
			}
		}
		for (i = 0; i < SQR(SHIFTWINSIZE); i++) {
			n_distance_energy[i] = (n_distance_energy[i] - min) / (max - min);
		}



		/* E. Calculate Final Energy */
		for (i = 0; i < SHIFTWINSIZE * SHIFTWINSIZE; i++) {
			p_final_energy[i] = p_distance_energy[i] + 1 * p_external_energy[i];
			n_final_energy[i] = n_distance_energy[i] + 1 * n_external_energy[i];
		}
		/* F. Store Next Location for Current Contour Point */

		min = 10000;
		int minima_found = 0;
		int minx = 0;
		int miny = 0;
		for (dr = -SHIFTWINSIZE / 2; dr <= SHIFTWINSIZE / 2; dr++) {
			for (dc = -SHIFTWINSIZE / 2; dc <= SHIFTWINSIZE / 2; dc++) {

				if (p_final_energy[(dr + (SHIFTWINSIZE / 2)) * SHIFTWINSIZE + (dc + (SHIFTWINSIZE / 2))] == min) {
					minima_found++;
					minx += prev_cont_x + dc;
					miny += prev_cont_y + dr;
				}
				else if (p_final_energy[(dr + (SHIFTWINSIZE / 2)) * SHIFTWINSIZE + (dc + (SHIFTWINSIZE / 2))] < min) {
					minx = prev_cont_x + dc;
					miny = prev_cont_y + dr;
					min = p_final_energy[(dr + (SHIFTWINSIZE / 2)) * SHIFTWINSIZE + (dc + (SHIFTWINSIZE / 2))];
					minima_found = 1;
				}
			}
		}




		int NextX = minx / minima_found;
		int NextY = miny / minima_found;

		// reset color before moving point

		int xPos = prev_cont_x;
		int yPos = prev_cont_y;

		for (dx = -2; dx <= 2; dx++) {
			for (dy = -2; dy <= 2; dy++) {
				if (yPos + dy < 0 || yPos + dy >= ROWS || xPos + dx < 0 || xPos + dx >= COLS) {
					og_color = 255;
				}
				else {
					og_color = OriginalImage[(yPos + dy) * COLS + xPos + dx];
				}
				SetPixel(hDC, xPos + dx, yPos + dy, RGB(og_color, og_color, og_color));
				contour_data[(yPos + dy) * COLS + xPos + dx] = -1;
				contour_point_data[(yPos + dy) * COLS + xPos + dx] = -1;

			}
		}

		prev_cont_x = NextX;
		prev_cont_y = NextY;


		/* Draw On the New Contour Points */
		xPos = prev_cont_x;
		yPos = prev_cont_y;

		for (dx = -2; dx <= 2; dx++) {
			for (dy = -2; dy <= 2; dy++) {
				SetPixel(hDC, xPos + dx, yPos + dy, RGB(255, 0, 0));
				contour_data[(yPos + dy) * COLS + xPos + dx] = contour_num;
				contour_point_data[(yPos + dy) * COLS + xPos + dx] = contour_point_num - 1;
			}
		}

		/* Next */

		min = 10000;
		minima_found = 0;
		minx = 0;
		miny = 0;
		for (dr = -SHIFTWINSIZE / 2; dr <= SHIFTWINSIZE / 2; dr++) {
			for (dc = -SHIFTWINSIZE / 2; dc <= SHIFTWINSIZE / 2; dc++) {

				if (n_final_energy[(dr + (SHIFTWINSIZE / 2)) * SHIFTWINSIZE + (dc + (SHIFTWINSIZE / 2))] == min) {
					minima_found++;
					minx += nxt_cont_x + dc;
					miny += nxt_cont_y + dr;
				}
				else if (n_final_energy[(dr + (SHIFTWINSIZE / 2)) * SHIFTWINSIZE + (dc + (SHIFTWINSIZE / 2))] < min) {
					minx = nxt_cont_x + dc;
					miny = nxt_cont_y + dr;
					min = n_final_energy[(dr + (SHIFTWINSIZE / 2)) * SHIFTWINSIZE + (dc + (SHIFTWINSIZE / 2))];
					minima_found = 1;
				}
			}
		}


		NextX = minx / minima_found;
		NextY = miny / minima_found;

		// reset color before moving point

		xPos = nxt_cont_x;
		yPos = nxt_cont_y;

		for (dx = -2; dx <= 2; dx++) {
			for (dy = -2; dy <= 2; dy++) {
				if (yPos + dy < 0 || yPos + dy >= ROWS || xPos + dx < 0 || xPos + dx >= COLS) {
					og_color = 255;
				}
				else {
					og_color = OriginalImage[(yPos + dy) * COLS + xPos + dx];
				}
				SetPixel(hDC, xPos + dx, yPos + dy, RGB(og_color, og_color, og_color));
				contour_data[(yPos + dy) * COLS + xPos + dx] = -1;
				contour_point_data[(yPos + dy) * COLS + xPos + dx] = -1;
			}
		}

		nxt_cont_x = NextX;
		nxt_cont_y = NextY;

		/* Draw On the New Contour Points */
		xPos = nxt_cont_x;
		yPos = nxt_cont_y;
		for (dx = -2; dx <= 2; dx++) {
			for (dy = -2; dy <= 2; dy++) {
				SetPixel(hDC, xPos + dx, yPos + dy, RGB(255, 0, 0));
				contour_data[(yPos + dy) * COLS + xPos + dx] = contour_num;
				contour_point_data[(yPos + dy) * COLS + xPos + dx] = contour_point_num + 1;
			}
		}

		/* if iterations exceeds max, break */
		iterations++;
		if (iterations == 3) {
			break;
		}

	}


	// end
	ContoursX[contour_num][contour_point_num] = end_x;
	ContoursY[contour_num][contour_point_num] = end_y;
	ContoursX[contour_num][(contour_point_num + 1) % contour_size] = nxt_cont_x;
	ContoursY[contour_num][(contour_point_num + 1) % contour_size] = nxt_cont_y;
	ContoursX[contour_num][(contour_point_num - 1) % contour_size] = prev_cont_x;
	ContoursY[contour_num][(contour_point_num - 1) % contour_size] = prev_cont_y;



	ReleaseDC(MainWnd, hDC);
}


void print_energy(double* energy) {
	int i, j;
	FILE* fpt;

	fpt = fopen("test2.txt", "w");

	for (i = 0; i < WINSIZE; i++) {
		for (j = 0; j < WINSIZE; j++) {
			fprintf(fpt, "%0.2lf ", energy[i * WINSIZE + j]);
		}
		fprintf(fpt, "\n");
	}
	fprintf(fpt, "\n");

}

double distance_between(int x1, int y1, int x2, int y2) {

	double distance;
	distance = SQR(x2 - x1) + SQR(y2 - y1);
	distance = sqrt(distance);
	return distance;
}


void normalize_energy(double* energy) {

	double max = -10000;
	double min = INT_MAX;
	int i, r, c;

	for (r = 0; r < WINSIZE; r++) {
		for (c = 0; c < WINSIZE; c++) {
			if (energy[r * WINSIZE + c] > max)
				max = energy[r * WINSIZE + c];
			else if (energy[r * WINSIZE + c] < min)
				min = energy[r * WINSIZE + c];
		}
	}
	for (i = 0; i < SQR(WINSIZE); i++) {
		energy[i] = (energy[i] - min) / (max - min);
	}


}


void normalize_sobel_image() {
	double max = -10000;
	double min = INT_MAX;
	int i, r, c;

	for (r = 1; r < ROWS - 1; r++) {
		for (c = 1; c < COLS - 1; c++) {

			if (sobel_image[r * COLS + c] < min)
				min = sobel_image[r * COLS + c];
			else if (sobel_image[r * COLS + c] > max) {
				max = sobel_image[r * COLS + c];
			}
		}
	}

	for (i = 0; i < ROWS * COLS; i++) {
		normalized_sobel[i] = (unsigned char)((sobel_image[i] - min) * (255) / (max - min));
	}
}

/* pad the sobel image so that window doesn't reach out on image */
void pad_sobel_image() {

	int r, c;

	// pad by 10 pixels
	unsigned char* padded_sobel;
	padded_sobel = (unsigned char*)calloc((ROWS + 20) * (COLS + 20), sizeof(unsigned char));

	for (r = -10; r < ROWS + 10; r++) {
		for (c = -10; c < COLS; c++) {
			if (r < 0 || r >= ROWS || c < 0 || c >= COLS) {
				padded_sobel[(r + 10) * (COLS + 20) + (c + 10)] = 0;
			}
			else {
				padded_sobel[(r + 10) * (COLS + 20) + (c + 10)] = normalized_sobel[r * COLS + c];
			}
		}
	}

	free(normalized_sobel);
	normalized_sobel = padded_sobel;
}



unsigned char* ScaleImage(unsigned char* img) {
	unsigned char* new_img;
	new_img = (unsigned char*)calloc(ROWS * COLS / 4, sizeof(unsigned char));


	int i, j, new_cols;

	int r_index = 0, c_index = 0;

	new_cols = COLS / 2;

	for (i = 0; i < ROWS; i++) {

		if ((i % 2) == 1) continue;

		c_index = 0;
		for (j = 0; j < COLS; j++) {

			if ((j % 2) == 1) continue;

			//printf("%d %d\n", i, j);

			new_img[r_index * new_cols + c_index] = img[i * COLS + j];
			c_index++;
		}

		r_index++;
	}

	COLS /= 2;
	ROWS /= 2;

	free(img);
	return new_img;


}


BOOL CALLBACK DlgProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam) {

	BOOL flag;
	char DWEIGHT[100], VWEIGHT[100], GRWEIGHT[100], SGROW[100];
	sprintf(DWEIGHT, "%.3f", DISWEIGHT);
	sprintf(VWEIGHT, "%.3f", DEVWEIGHT);
	sprintf(GRWEIGHT, "%.3f", EXTWEIGHT);



	switch (Message)
	{
	case WM_INITDIALOG:

		SetDlgItemInt(hWnd, IDC_WINSIZE, WINSIZE, FALSE);
		SetDlgItemText(hWnd, IDC_DISWEIGHT, DWEIGHT);
		SetDlgItemText(hWnd, IDC_VARWEIGHT, VWEIGHT);
		SetDlgItemText(hWnd, IDC_GRWEIGHT, GRWEIGHT);
		SetDlgItemInt(hWnd, IDC_ITERATIONS, MAXITERATIONS, FALSE);

		if (SLOWGROW) {
			SetDlgItemText(hWnd, IDC_SLOWGROW, "y");
		}
		else {
			SetDlgItemText(hWnd, IDC_SLOWGROW, "n");

		}

		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			WINSIZE = GetDlgItemInt(hWnd, IDC_WINSIZE, &flag, FALSE);
			GetDlgItemText(hWnd, IDC_DISWEIGHT, DWEIGHT, 10);
			GetDlgItemText(hWnd, IDC_VARWEIGHT, VWEIGHT, 10);
			GetDlgItemText(hWnd, IDC_GRWEIGHT, GRWEIGHT, 10);
			MAXITERATIONS = GetDlgItemInt(hWnd, IDC_ITERATIONS, &flag, FALSE);
			GetDlgItemText(hWnd, IDC_SLOWGROW, SGROW, 10);

			if (strcmp(SGROW, "y") == 0) {
				SLOWGROW = 1;
			}
			else {
				SLOWGROW = 0;
			}

			DISWEIGHT = atof(DWEIGHT);
			DEVWEIGHT = atof(VWEIGHT);
			EXTWEIGHT = atof(GRWEIGHT);

			EndDialog(hWnd, wParam);
			break;
		case IDCANCEL:
			EndDialog(hWnd, wParam);
			break;
		}
		break;
	default:
		return FALSE;
	}

	return TRUE;
}


