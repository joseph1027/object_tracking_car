#include <opencv2/core/utility.hpp>
#include "opencv2/video/tracking.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/videoio.hpp"
#include "opencv2/highgui.hpp"
#include <sstream>
#include <iostream>
#include <stdlib.h>
#include <ctype.h>
#include <thread>


#define CURL_STATICLIB
#include"curl/curl.h"

#ifdef _DEBUG
#	pragma comment (lib,"curl/libcurl_a_debug.lib")
#else
#	pragma comment (lib,"curl/libcurl_a.lib")
#endif



using namespace cv;
using namespace std;

Mat image;

int prevv;
bool backprojMode = false;
bool selectObject = false;
int trackObject = 0;
bool showHist = true;
Point origin;
Rect selection;
int vmin = 10, vmax = 255, smin = 30, smax = 255;

string intToString(int number) {
	std::stringstream ss;
	ss << number;
	return ss.str();
}

static void onMouse(int event, int x, int y, int, void*)
{
	if (selectObject)
	{
		selection.x = MIN(x, origin.x);
		selection.y = MIN(y, origin.y);
		selection.width = std::abs(x - origin.x);
		selection.height = std::abs(y - origin.y);

		selection &= Rect(0, 0, image.cols, image.rows);
	}

	switch (event)
	{
	case EVENT_LBUTTONDOWN:
		origin = Point(x, y);
		selection = Rect(x, y, 0, 0);
		selectObject = true;
		break;
	case EVENT_LBUTTONUP:
		selectObject = false;
		if (selection.width > 0 && selection.height > 0)
			trackObject = -1;
		break;
	}
}

string hot_keys =
"\n\nHot keys: \n"
"\tESC - quit the program\n"
"\tc - stop the tracking\n"
"\tb - switch to/from backprojection view\n"
"\th - show/hide object histogram\n"
"\tp - pause video\n"
"To initialize tracking, select the object with mouse\n";



int main(int argc, const char** argv)
{	
	/*thread second(goo, 20);
	thread first (foo);
	
	first.join();
	second.join();
	cout << "threads done!!!" << endl;*/

	prevv = 2;
	CURL *curl;
	CURLcode res;
	curl = curl_easy_init();
	if (curl)
	{
		cout << "YOOOOOOOOOOOOOO" << endl;
		curl_easy_setopt(curl, CURLOPT_URL, "http://arduino2.local/arduino/digital/13/0");
		res = curl_easy_perform(curl);
		//curl_easy_cleanup(curl);
	}
	
	VideoCapture cap;
	Rect trackWindow;
	int hsize = 16;
	float hranges[] = { 0,180 };
	const float* phranges = hranges;
	//CommandLineParser parser(argc, argv, keys);
	/*if (parser.has("help"))
	{
	help();
	return 0;
	}*/
	//int camNum = parser.get<int>(0);

	cap.open(0);//"http://arduino.local:8080/?action=stream"
	//VideoCapture cap("234.mp4");//234.mp4
	cout << hot_keys;
	namedWindow("Histogram", 0);
	namedWindow("CamShift Demo", 0);
	setMouseCallback("CamShift Demo", onMouse, 0);
	createTrackbar("Vmin", "CamShift Demo", &vmin, 255, 0);
	createTrackbar("Vmax", "CamShift Demo", &vmax, 255, 0);
	createTrackbar("Smin", "CamShift Demo", &smin, 255, 0);
	createTrackbar("Smax", "CamShift Demo", &smax, 255, 0);

	Mat frame, hsv, hue, mask, hist, histimg = Mat::zeros(200, 320, CV_8UC3), backproj;
	bool paused = false;

	while (1)
	{
		if (!paused)
		{
			cap >> frame;
		}

		frame.copyTo(image);

		if (!paused)
		{
			cvtColor(image, hsv, COLOR_BGR2HSV);

			if (trackObject)
			{
				int _vmin = vmin, _vmax = vmax;
				int _smin = smin, _smax = smax;
				inRange(hsv, Scalar(0, MIN(_smin, _smax), MIN(_vmin, _vmax)),
					Scalar(180, MAX(_smin, _smax), MAX(_vmin, _vmax)), mask);
				int ch[] = { 0, 0 };
				hue.create(hsv.size(), hsv.depth());
				mixChannels(&hsv, 1, &hue, 1, ch, 1);

				if (trackObject < 0)
				{
					Mat roi(hue, selection), maskroi(mask, selection);
					calcHist(&roi, 1, 0, maskroi, hist, 1, &hsize, &phranges);
					normalize(hist, hist, 0, 255, NORM_MINMAX);

					trackWindow = selection;
					trackObject = 1;

					histimg = Scalar::all(0);
					int binW = histimg.cols / hsize;
					Mat buf(1, hsize, CV_8UC3);
					for (int i = 0; i < hsize; i++)
						buf.at<Vec3b>(i) = Vec3b(saturate_cast<uchar>(i*180. / hsize), 255, 255);
					cvtColor(buf, buf, COLOR_HSV2BGR);

					for (int i = 0; i < hsize; i++)
					{
						int val = saturate_cast<int>(hist.at<float>(i)*histimg.rows / 255);
						rectangle(histimg, Point(i*binW, histimg.rows),
							Point((i + 1)*binW, histimg.rows - val),
							Scalar(buf.at<Vec3b>(i)), -1, 8);
					}
				}
				calcBackProject(&hue, 1, 0, hist, backproj, &phranges);
				backproj &= mask;
				RotatedRect trackBox = CamShift(backproj, trackWindow,
					TermCriteria(TermCriteria::EPS | TermCriteria::COUNT, 10, 1));
				if (trackWindow.area() <= 1)
				{
					int cols = backproj.cols, rows = backproj.rows, r = (MIN(cols, rows) + 5) / 6;
					trackWindow = Rect(trackWindow.x - r, trackWindow.y - r,
						trackWindow.x + r, trackWindow.y + r) &
						Rect(0, 0, cols, rows);
				}

				if (backprojMode)
					cvtColor(backproj, image, COLOR_GRAY2BGR);
				ellipse(image, trackBox, Scalar(0, 255, 0), 2, LINE_AA);
				cout << trackBox.center << " ";
				cout << "{" << trackBox.size.height << " , ";
				cout << trackBox.size.width << "}" << endl;
				circle(image, trackBox.center, 2, Scalar(0, 0, 255), 2);
				putText(image, intToString(trackBox.center.x) + "," + intToString(trackBox.center.y), trackBox.center, 1, 1, Scalar(0, 0, 255), 2);
				if ((trackBox.center.x < 320 && prevv == 0) || prevv == 2) {
					curl_easy_setopt(curl, CURLOPT_URL, "http://arduino2.local/arduino/digital/12/0");
					res = curl_easy_perform(curl);
					curl_easy_setopt(curl, CURLOPT_URL, "http://arduino2.local/arduino/digital/10/1");
					res = curl_easy_perform(curl);
					/*curl_easy_setopt(curl, CURLOPT_URL, "http://arduino2.local/arduino2/digital/10/0");
					res = curl_easy_perform(curl);*/
					prevv = 1;
				}
				else if ((trackBox.center.x > 320 && prevv == 1) || prevv == 2 )//&& prevv == 1)||prevv==2
				{
					curl_easy_setopt(curl, CURLOPT_URL, "http://arduino2.local/arduino/digital/10/0");
					res = curl_easy_perform(curl);
					curl_easy_setopt(curl, CURLOPT_URL, "http://arduino2.local/arduino/digital/12/1");
					res = curl_easy_perform(curl);
					/*curl_easy_setopt(curl, CURLOPT_URL, "http://arduino2.local/arduino2/digital/12/0");
					res = curl_easy_perform(curl);*/
					prevv = 0;
				}
			}
		}
		else if (trackObject < 0)
			paused = false;

		if (selectObject && selection.width > 0 && selection.height > 0)
		{
			Mat roi(image, selection);
			bitwise_not(roi, roi);
		}

		imshow("CamShift Demo", image);
		imshow("Histogram", histimg);

		char c = (char)waitKey(10);
		if (c == 27)
			break;
		switch (c)
		{
		case 'b':
			backprojMode = !backprojMode;
			break;
		case 'c':
			trackObject = 0;
			histimg = Scalar::all(0);
			break;
		case 'h':
			showHist = !showHist;
			if (!showHist)
				destroyWindow("Histogram");
			else
				namedWindow("Histogram", 1);
			break;
		case 'p':
			paused = !paused;
			break;
		default:
			;
		}
	}

	return 0;
}
