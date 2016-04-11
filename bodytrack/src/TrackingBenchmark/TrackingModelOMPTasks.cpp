//-------------------------------------------------------------
//      ____                        _      _
//     / ___|____ _   _ ____   ____| |__  | |
//    | |   / ___| | | |  _  \/ ___|  _  \| |
//    | |___| |  | |_| | | | | |___| | | ||_|
//     \____|_|  \_____|_| |_|\____|_| |_|(_) Media benchmarks
//                  
//	  2006, Intel Corporation, licensed under Apache 2.0 
//
//  file : TrackingModelOMPTasks.cpp
//  author : Scott Ettinger - scott.m.ettinger@intel.com
//  description : Observation model for kinematic tree body 
//				  tracking threaded with OpenMP.
//				  
//  modified : Dimitrios Chasapis - dimitrios.chasapis@bsc.es
//--------------------------------------------------------------

#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif

#include "TrackingModelOMPTasks.h"
#include <vector>
#include <string>
#include "system.h"
#include <cstdio>
//#include "/apps/CEPBATOOLS/extrae/latest/default/64/include/extrae_user_events.h"

#include <omp.h>

#include <sys/time.h>

/* Timing stuff */
typedef struct timeval timer;
#define TIME(X) gettimeofday(&X, NULL);

#define GRAIN_SIZE 8

using namespace std;

extern int work_units;

//------------------------ Threaded versions of image filters --------------------
 
void DoFlexFilterRowVOMPSS(Im8u *src, Im8u *dst, float *kernel, int StepBytes, int Bpp, int n, int width, int bsize) {
	
	for(int y = 0; y < bsize; y++)
	{
// 	  Im8u *psrc = (Im8u *)((char *)src+StepBytes*y+Bpp*n); 
// 	  Im8u *pdst = (Im8u *)((char *)dst+StepBytes*y+Bpp*n);
	  Im8u *psrc = (Im8u *)((char *)src+StepBytes*y); 
	  Im8u *pdst = (Im8u *)((char *)dst+StepBytes*y);
	  for(int x = n; x < width - n; x++)
		{
			int k = 0; 
			float acc = 0; 
			for(int i = -n; i <= n; i++)
				acc += (float)(psrc[i] * kernel[k++]);

			*pdst = (float)acc;
			pdst++;
			psrc++;
		}
	}
}


//OMPSS threaded - 1D filter Row wise 1 channel any type data or kernel valid pixels only
bool FlexFilterRowVOMPSS(FlexImage<Im8u,1> &src, FlexImage<Im8u,1> &dst, float *kernel, int kernelSize, bool allocate = true)
{
	if(kernelSize % 2 == 0)									//enforce odd length kernels
		return false;
	if(allocate)
		dst.Reallocate(src.Size());
	dst.Set((Im8u)0);
	int n = kernelSize / 2, h = src.Height();

	int StepBytes = src.StepBytes();
	int Bpp = src.BytesPerPixel();
	int source_width = src.Width();
	Im8u *psrc = &src(0, 0); 
	Im8u *pdst = &dst(0, 0);
	//int np = omp_get_max_threads();
	int np = work_units;
	int remainder = h%np;
	int step = h/np;
// 	printf("Rows:h=%d - step=%d - remainder=%d - np=%d\n", h, step, remainder, np);
// 	printf("Bpp=%d\n", Bpp);
	for(int j=0; j < h; j+=step) {
		int bsize = (j+step)>h?remainder:step;
		int index = StepBytes*j+Bpp*n;
		#pragma omp task depend(out: pdst[index]) depend(in: psrc[index], kernel[0]) firstprivate(index, bsize) //label(FlexFilterRowVOMPSS)
		DoFlexFilterRowVOMPSS(&psrc[index], &pdst[index], &kernel[0], StepBytes, Bpp, n, source_width, bsize);
	}
	//#pragma omp taskwait
	return true;
}


void DoFlexFilterColumnVOMPSS(Im8u *src, Im8u *dst, float *kernel, int StepBytes, int Bpp, int n, int width, int bsize) {

	for(int y = 0; y < bsize; y++)
	{
// 		Im8u *psrc = (Im8u *)((char *)src+StepBytes*y+Bpp*0); 
// 		Im8u *pdst = (Im8u *)((char *)dst+StepBytes*y+Bpp*0);
		Im8u *psrc = (Im8u *)((char *)src+StepBytes*y); 
		Im8u *pdst = (Im8u *)((char *)dst+StepBytes*y);
		for(int x = 0; x < width; x++)
		{
			int k = 0; 
			float acc = 0; 
			for(int i = -n; i <= n; i++)
				acc += (float)(*(Im8u *)((char *)psrc + StepBytes * i) * kernel[k++]); 
			*pdst = (Im8u)acc;
			pdst++;
			psrc++;
		}
	}
}

//OMPSS threaded - 1D filter Column wise 1 channel any type data or kernel valid pixels only
bool FlexFilterColumnVOMPSS(FlexImage<Im8u,1> &src, FlexImage<Im8u,1> &dst, float *kernel, int kernelSize, bool allocate = true)
{
	if(kernelSize % 2 == 0)									//enforce odd length kernels
		return false;
	if(allocate)
		dst.Reallocate(src.Size());
	dst.Set((Im8u)0);
	int n = kernelSize / 2;
	int h = src.Height() - n;
	
	int StepBytes = src.StepBytes();
	int Bpp = src.BytesPerPixel();
	int source_width = src.Width();
	Im8u *psrc = &src(0, 0); 
	Im8u *pdst = &dst(0, 0);
	//int np = omp_get_max_threads();
	int np = work_units;
	int step = h/np;
	int remainder = h%np;
	//printf("Cols:h=%d - step=%d - remainder=%d - np=%d\n", h, step, remainder, np);
	for(int j=0; j < h; j+=step) {
		int bsize = (j+step)>h?remainder:step;
		int index = StepBytes*j;
		#pragma omp task depend(out: pdst[index]) depend(in: psrc[index], kernel[0]) firstprivate(index, bsize) //label(FlexFilterColumnVOMPSS)
		DoFlexFilterColumnVOMPSS(&psrc[index], &pdst[index], &kernel[0], StepBytes, Bpp, n, source_width, bsize);
	}
	//#pragma omp taskwait
	return true;
}

// ----------------------------------------------------------------------------------

//Generate an edge map from the original camera image
//Separable 7x7 gaussian filter - threaded
inline void GaussianBlurOMPSS(FlexImage8u &src, FlexImage8u &dst)
{
	float k[] = {0.12149085090552f, 0.14203719483447f, 0.15599734045770f, 0.16094922760463f, 0.15599734045770f, 0.14203719483447f, 0.12149085090552f};
	FlexImage8u tmp;
	FlexFilterRowVOMPSS(src, tmp, k, 7);											//separable gaussian convolution using kernel k
	FlexFilterColumnVOMPSS(tmp, dst, k, 7);
	//#pragma omp taskwait
}

void DoGradientMagThresholdOMPSS(Im8u *src, Im8u *tmp, float threshold,
																	int StepBytes, int Bpp, int width, int bsize) {

	for(int y = 1; y < bsize; y++) {
// 		Im8u *p = (Im8u *)((char *)src+StepBytes*y+Bpp*1); 
// 		Im8u *ph = (Im8u *)((char *)src+StepBytes*(y-1)+Bpp*1);
// 		Im8u *pl = (Im8u *)((char *)src+StepBytes*(y+1)+Bpp*1);
// 		Im8u *pr = (Im8u *)((char *)tmp+StepBytes*y+Bpp*1); 
		Im8u *p = (Im8u *)((char *)src+StepBytes*y); 
		Im8u *ph = (Im8u *)((char *)src+StepBytes*(y-1));
		Im8u *pl = (Im8u *)((char *)src+StepBytes*(y+1));
		Im8u *pr = (Im8u *)((char *)tmp+StepBytes*y); 
		for(int x = 1; x < width - 1; x++)
		{	
			float xg = -0.125f * ph[-1] + 0.125f * ph[1] - 0.250f * p[-1] + 0.250f * p[1] - 0.125f * pl[-1] + 0.125f * pl[1];	//calc x and y gradients
			float yg = -0.125f * ph[-1] - 0.250f * ph[0] - 0.125f * ph[1] + 0.125f * pl[-1] + 0.250f * pl[0] + 0.125f * pl[1];
			float mag = xg * xg + yg * yg;																					//calc magnitude and threshold
			*pr = (mag < threshold) ? 0 : 255;
			p++; ph++; pl++; pr++;
		}
	}
}

//Calculate gradient magnitude and threshold to binarize - threaded
inline void GradientMagThresholdOMPSS(FlexImage8u &src, FlexImage8u &r, float threshold)
{
	//FlexImage8u r(src.Size());
	ZeroBorder(r);
	
	int StepBytes = src.StepBytes();
	int Bpp = src.BytesPerPixel();
	int source_width = src.Width();
	Im8u *psrc = &src(0, 0); 
	Im8u *pr = &r(0, 0);
	//int np = omp_get_max_threads();
	int np = work_units;
	int step = src.Height()/np;
	int remainder = src.Height()%np;
	
	for(int j=1; j < src.Height()-1; j+=step) {
		int bsize = ((j+step)>(src.Height()-1))?remainder:step;
		int index = StepBytes*(j-1)+Bpp*1;
		#pragma omp task depend(out: pr[index]) depend(in: psrc[index]) firstprivate(index, bsize) //label(GradientMagThresholdOMPSS)
		DoGradientMagThresholdOMPSS(&psrc[index], &pr[index], threshold, StepBytes, Bpp, source_width, bsize);
	}
	//#pragma omp taskwait

	//return r;
}

TrackingModelOMPTasks::TrackingModelOMPTasks() {
	return;
}

TrackingModelOMPTasks::~TrackingModelOMPTasks() {
	return;
}

//Generate an edge map from the original camera image
void TrackingModelOMPTasks::CreateEdgeMap(FlexImage8u &src, FlexImage8u &dst)
{
	printf("YOU SUMMONED ME?\n");
	//FIXME: return value maybe better used as inoutargument (aliases with src though)
	//FlexImage8u gr = GradientMagThresholdOMPSS(src, 16.0f);						//calc gradient magnitude and threshold
	//GaussianBlurOMPSS(gr, dst);													//Blur to create distance error map
	//#pragma omp taskwait
}

//templated conversion to string with field width
template<class T>
inline string str(T n, int width = 0, char pad = '0')
{	stringstream ss;
	ss << setw(width) << setfill(pad) << n;
	return ss.str();
}

/*
*   Function: timevaldiff
*   ---------------------
*   Calculates the time difference between start and finish in msecs.
*/
long timevaldiff2(timer* start, timer* finish){
    long msec;
    msec = (finish->tv_sec - start->tv_sec)*1000;
    msec += (finish->tv_usec - start->tv_usec)/1000;
    return msec;
}

//load and process all images for new observation at a given time(frame)
//Overloaded from base class for future threading to overlap disk I/O with 
//generating the edge maps
bool TrackingModelOMPTasks::GetObservation(float timeval)
{
	int frame = (int)timeval;													//generate image filenames
	int n = mCameras.GetCameraCount();
	vector<string> FGfiles(n), ImageFiles(n);
	for(int i = 0; i < n; i++)													
	{	FGfiles[i] = mPath + "FG" + str(i + 1) + DIR_SEPARATOR + "image" + str(frame, 4) + ".bmp";
		ImageFiles[i] = mPath + "CAM" + str(i + 1) + DIR_SEPARATOR + "image" + str(frame, 4) + ".bmp";
	}
	FlexImage8u im[(int)FGfiles.size()]; //FIXME: this could probably be an array, so we can parallelize this?
	bool error[(int)FGfiles.size()];
	string errorFile[(int)FGfiles.size()];
	BinaryImage* _mFGMaps = &mFGMaps[0];
	FlexImage8u* _mEdgeMaps = &mEdgeMaps[0];
	string* _FGfiles = &FGfiles[0];
	string* _ImageFiles = &ImageFiles[0];
	FlexImage8u gr[(int)FGfiles.size()]; 
	FlexImage8u tmp[(int)FGfiles.size()];
	for(int i = 0; i < (int)FGfiles.size(); i++)
	{
		//Extrae_user_function(666);
		//printf("Creating task\n");
		#pragma omp task depend(inout: _mFGMaps[i]) depend(out: _mEdgeMaps[i], error[i], errorFile[i]) depend(in: im[i], _FGfiles[i], _ImageFiles[i], gr[i], tmp[i]) firstprivate(i) //label(GetObservation)
		{
			error[i] = true;

			//#pragma omp task inout(errorFile[i], error[i], im[i], _FGfiles[i], _mFGMaps[i], ImageFiles[i]) label(loadImage)
			{
				if(!FlexLoadBMP(_FGfiles[i].c_str(), im[i]))								//Load foreground maps and raw images
				{	//cout << "Unable to load image: " << FGfiles[i].c_str() << endl;
					//return false;
					errorFile[i] = _FGfiles[i].c_str();
					error[i] = false;
				}

				_mFGMaps[i].ConvertToBinary(im[i]);											//binarize foreground maps to 0 and 1

				if(!FlexLoadBMP(_ImageFiles[i].c_str(), im[i]))
				{	//cout << "Unable to load image: " << ImageFiles[i].c_str() << endl;
					errorFile[i] = _ImageFiles[i].c_str();
					error[i] = false;
				}
			}
			//#pragma omp task inout(im[i], gr[i], tmp[i], _mEdgeMaps[i]) firstprivate(i) label(parSection)
			{
				gr[i].Reallocate(im[i].Size());
				GradientMagThresholdOMPSS(im[i], gr[i], 16.0f);						//calc gradient magnitude and threshold
				float k[] = {0.12149085090552f, 0.14203719483447f, 0.15599734045770f, 0.16094922760463f, 0.15599734045770f, 0.14203719483447f, 0.12149085090552f};
				FlexFilterRowVOMPSS(gr[i], tmp[i], k, 7);											//separable gaussian convolution using kernel k
				FlexFilterColumnVOMPSS(tmp[i], _mEdgeMaps[i], k, 7);
			}	
		}
	}
	#pragma omp taskwait
// 	for(int i = 0; i < (int)FGfiles.size(); i++) {
// 		if(error[i] == false)  {
// 			cout << "Unable to load image: " << errorFile[i].c_str() << endl;
// 			return false;
// 		}
// 	}
	return true;
}
