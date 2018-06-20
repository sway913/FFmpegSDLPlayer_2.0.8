#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/log.h"

#include "mediacodec/mediacodec.h"

#ifdef ANDROID
#include <jni.h>
#include <android/log.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO , "libSDL2", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR , "libSDL2", __VA_ARGS__)
#else
#define LOGE(format, ...)  printf("libSDL2" format "\n", ##__VA_ARGS__)
#define LOGI(format, ...)  printf("libSDL2" format "\n", ##__VA_ARGS__)
#endif

#include "SDL.h"
#include "SDL_log.h"
#include "SDL_main.h"

#define SDL_JAVA_PREFIX_CLASS                           "com/example/ffmpegsdlplayer/activity/SDLActivity"
#define SDL_JAVA_PREFIX                                 com_example_ffmpegsdlplayer_activity
#define CONCAT1(prefix, class, function)                CONCAT2(prefix, class, function)
#define CONCAT2(prefix, class, function)                Java_ ## prefix ## _ ## class ## _ ## function
#define SDL_JAVA_INTERFACE(function)                    CONCAT1(SDL_JAVA_PREFIX, SDLActivity, function)
#define SDL_JAVA_AUDIO_INTERFACE(function)              CONCAT1(SDL_JAVA_PREFIX, SDLAudioManager, function)
#define SDL_JAVA_CONTROLLER_INTERFACE(function)         CONCAT1(SDL_JAVA_PREFIX, SDLControllerManager, function)
#define SDL_JAVA_INTERFACE_INPUT_CONNECTION(function)   CONCAT1(SDL_JAVA_PREFIX, SDLInputConnection, function)

//Refresh Event
#define REFRESH_EVENT  (SDL_USEREVENT + 1)
//Break
#define BREAK_EVENT  (SDL_USEREVENT + 2)

#define MEDIACODEC_DEC_BUFFER_ARRAY_SIZE  1024*1024*1
#define MEDIACODEC_DEC_YUV_ARRAY_SIZE  2000*1200*3/2
#define MEDIACODEC_ANDROID  0

volatile int sdl_delay = 40;
volatile int thread_exit=0;
volatile int thread_back=0;
volatile int thread_pause=0;
int thread_forward=0;
int thread_backward=0;

int thread_picture=0;
int thread_video=0;

int thread_codec_type = 0;

long long skipFrame;
int thread_Seek=0;
long long seekFrame;
int is_Locating_I=0;
SDL_Rect sdlRect;
SDL_Point sdlPoint;
int screen_w=640,screen_h=480;
int pixel_w=640,pixel_h=480;
const int bpp=12;
int angle=0;
int zoom=0;

void updateSdlRect(int zoom){
	if(angle == 0 || angle == 180){
		if(zoom == 1) //原始
		{
			sdlRect.w = pixel_w;
			sdlRect.h = pixel_h;
			sdlRect.x = (screen_w-sdlRect.w)/2;
			sdlRect.y = (screen_h-sdlRect.h)/2;
			
			sdlPoint.x = sdlRect.w / 2;
			sdlPoint.y = sdlRect.h / 2;
		}
		else if(zoom == 2) //拉伸
		{
			sdlRect.w = screen_w;
			sdlRect.h = screen_h;
			sdlRect.x = 0;
			sdlRect.y = 0;
			
			sdlPoint.x = sdlRect.w / 2;
			sdlPoint.y = sdlRect.h / 2;
		}
		else if(zoom == 0) //适应屏幕
		{
			double w_times = (double)screen_w/(double)pixel_w;
			double h_times = (double)screen_h/(double)pixel_h;
			if(w_times > h_times)
			{
				sdlRect.w = (int)(pixel_w * h_times);
				sdlRect.h = screen_h;
				sdlRect.x = (screen_w-(int)(pixel_w * h_times))/2;
				sdlRect.y = 0;
			}
			else
			{
				sdlRect.w = screen_w;
				sdlRect.h = (int)(pixel_h * w_times);
				sdlRect.x = 0;
				sdlRect.y = (screen_h-(int)(pixel_h * w_times))/2;
			}
			
			sdlPoint.x = sdlRect.w / 2;
			sdlPoint.y = sdlRect.h / 2;
		}
	}
	else if(angle == 90 || angle == 270){
		if(zoom == 1) //原始
		{
			sdlRect.w = pixel_w;
			sdlRect.h = pixel_h;
			sdlRect.x = (screen_w-sdlRect.w)/2;
			sdlRect.y = (screen_h-sdlRect.h)/2;
			
			sdlPoint.x = sdlRect.w / 2;
			sdlPoint.y = sdlRect.h / 2;
		}
		else if(zoom == 2) //拉伸
		{
			sdlRect.w = screen_h;
			sdlRect.h = screen_w;
			sdlRect.x = (screen_w-sdlRect.w)/2;
			sdlRect.y = (screen_h-sdlRect.h)/2;
			
			sdlPoint.x = sdlRect.w / 2;
			sdlPoint.y = sdlRect.h / 2;
		}
		else if(zoom == 0) //适应屏幕
		{
			double w_times = (double)screen_w/(double)pixel_h;
			double h_times = (double)screen_h/(double)pixel_w;
			if(w_times > h_times)
			{
				sdlRect.w = screen_h;
				sdlRect.h = (int)(pixel_h * h_times);
				sdlRect.x = (screen_w-sdlRect.w)/2;
				sdlRect.y = (screen_h-sdlRect.h)/2;
			}
			else
			{
				sdlRect.w = (int)(pixel_w * w_times);
				sdlRect.h = screen_w;
				sdlRect.x = (screen_w-sdlRect.w)/2;
				sdlRect.y = (screen_h-sdlRect.h)/2;
			}
			
			sdlPoint.x = sdlRect.w / 2;
			sdlPoint.y = sdlRect.h / 2;
		}
	}
}

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeBackSDLThread)(JNIEnv* env, jclass jcls)
{
	thread_back = 1;
}
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativePauseSDLThread)(JNIEnv* env, jclass jcls)
{
	thread_pause = 1;
}
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativePlaySDLThread)(JNIEnv* env, jclass jcls)
{
	thread_pause = 0;
}
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeZoomSDLThread)(JNIEnv* env, jclass jcls, jint jzoom)
{
	LOGI("zoom：%d",jzoom);
	updateSdlRect(jzoom);
	zoom = jzoom;
}
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeForwardSDLThread)(JNIEnv* env, jclass jcls, jlong jskipFrame)
{
	thread_forward = 1;
	is_Locating_I = 1;
	skipFrame = jskipFrame;
}
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeBackwardSDLThread)(JNIEnv* env, jclass jcls, jlong jskipFrame)
{
	thread_backward = 1;
	is_Locating_I = 1;
	skipFrame = jskipFrame;
}
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeSeekSDLThread)(JNIEnv* env, jclass jcls, jlong jseekFrame)
{
	thread_Seek = 1;
	is_Locating_I = 1;
	seekFrame = jseekFrame;
}
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativePictureSDLThread)(JNIEnv* env, jclass jcls)
{
	thread_picture=1;
}
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeStartVideoSDLThread)(JNIEnv* env, jclass jcls)
{
	thread_video=1;
}
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeEndVideoSDLThread)(JNIEnv* env, jclass jcls)
{
	thread_video=3;
}
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeCodecType)(JNIEnv* env, jclass jcls, jint jcodec_type)
{
	thread_codec_type=jcodec_type;
	LOGI("thread_codec_type = %d",thread_codec_type);
}
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeUpdateSdlRect)(JNIEnv* env, jclass jcls, jint jscreen_w, jint jscreen_h)
{
	screen_w = jscreen_w;
	screen_h = jscreen_h;
	updateSdlRect(zoom);
	LOGI("update : screen_w:%d  screen_h:%d",screen_w,screen_h);
}


void clearBreakEvent(){
	LOGI("清空所有变量");
	thread_exit = 0;
	thread_back = 0;
	
	thread_pause=0;
	thread_forward=0;
	thread_backward=0;

	thread_picture=0;
	thread_video=0;

	thread_Seek=0;
	is_Locating_I=0;
	SDL_Event event;
	while(SDL_PollEvent(&event)){

	}
}

int refresh_video(void *opaque)
{
	LOGI("refresh_video(void *opaque) thread_exit=%d thread_back=%d",thread_exit,thread_back);
	while (thread_exit==0 && thread_back==0)
	{
		// LOGI("SDL_PushEvent() REFRESH_EVENT=%d",REFRESH_EVENT);
		SDL_Event event;
		// while(SDL_PollEvent(&event)){
			// if(event.type != REFRESH_EVENT){
				// SDL_PushEvent(&event);
				// break;
			// }
		// }
		event.type = REFRESH_EVENT;
		SDL_PushEvent(&event);
		if(!thread_pause)
		{
			SDL_Delay(sdl_delay);
		}
		else
		{
			while(thread_pause)
			{
				SDL_Delay(sdl_delay);
			}
		}
	}
	LOGI("SDL_PushEvent() BREAK_EVENT=%d",BREAK_EVENT);
	//Break
	SDL_Event event;
	event.type = BREAK_EVENT;
	SDL_PushEvent(&event);
	return 0;
}

// static int CheckInterrupt(void* ctx)
// {
// 　　return  ? 1 : 0;//3秒超时
// }

// void frame_rotate_90( AVFrame *src,AVFrame*des)    
// {    
    // int n = 0,i= 0,j = 0; 
    // int hw = src->width>>1;  
    // int hh = src->height>>1;  
    // int size = src->width * src->height;  
    // int hsize = size>>2;  
      
    // int pos = 0;  
    // //copy y    
    // for(j = 0; j < src->width;j++)    
    // {    
    // pos = size;  
        // for(i = src->height - 1; i >= 0; i--)  
        // {   pos-=src->width;  
            // des->data[0][n++] = src->data[0][pos + j];  
        // }    
    // }  
    // //copy uv  
    // n = 0;  
    // for(j = 0;j < hw;j++)    
    // {   pos= hsize;  
        // for(i = hh - 1;i >= 0;i--)    
        // {  
        // pos-=hw;  
            // des->data[1][n] = src->data[1][ pos + j];  
        // des->data[2][n] = src->data[2][ pos + j];  
        // n++;  
        // }  
    // }  
      
    // des->linesize[0] = src->height;  
    // des->linesize[1] = src->height>>1;  
    // des->linesize[2] = src->height>>1;  
    // des->height = src->width;  
    // des->width = src->height;  
// } 

// void frame_rotate_180(AVFrame *src,AVFrame*des)  
// {  
    // int n = 0,i= 0,j = 0;
    // int hw = src->width>>1;  
    // int hh = src->height>>1;  
    // int pos= src->width * src->height;  
      
    // for (i = 0; i < src->height; i++)  
    // {  
        // pos-= src->width;  
        // for (j = 0; j < src->width; j++) {  
            // des->data[0][n++] = src->data[0][pos + j];  
        // }  
    // }  
  
    // n = 0;  
    // pos = src->width * src->height>>2;  
      
    // for (i = 0; i < hh;i++) {  
        // pos-= hw;  
        // for (j = 0; j < hw;j++) {  
              
            // des->data[1][n] = src->data[1][ pos + j];  
            // des->data[2][n] = src->data[2][ pos + j];  
            // n++;  
        // }  
    // }  
      
    // des->linesize[0] = src->width;  
    // des->linesize[1] = src->width>>1;  
    // des->linesize[2] = src->width>>1;  
      
    // des->width = src->width;  
    // des->height = src->height;  
    // des->format = src->format;  
      
    // des->pts = src->pts;  
    // des->pkt_pts = src->pkt_pts;  
    // des->pkt_dts = src->pkt_dts;  
      
    // des->key_frame = src->key_frame;  
// }  

// void frame_rotate_270(AVFrame *src,AVFrame*des)  
// {  
    // int n = 0,i= 0,j = 0;  
    // int hw = src->width>>1;  
    // int hh = src->height>>1;  
    // int pos = 0;  
      
    // for(i = src->width-1;i >= 0;i--)  
    // {  
        // pos = 0;  
        // for(j = 0;j < src->height;j++)  
        // {  
            // des->data[0][n++]= src->data[0][pos+i];  
            // pos += src->width;  
        // }  
    // }  
      
    // n = 0;  
    // for (i = hw-1; i >= 0;i--) {  
        // pos= 0;  
        // for (j = 0; j < hh;j++) {  
            // des->data[1][n]= src->data[1][pos+i];  
            // des->data[2][n]= src->data[2][pos+i];  
            // pos += hw;  
            // n++;  
        // }  
    // }  
      
    // des->linesize[0] = src->height;  
    // des->linesize[1] = src->height>>1;  
    // des->linesize[2] = src->height>>1;  
      
    // des->width = src->height;  
    // des->height = src->width;  
    // des->format = src->format;  
      
    // des->pts = src->pts;  
    // des->pkt_pts = src->pkt_pts;  
    // des->pkt_dts = src->pkt_dts;  
      
    // des->key_frame = src->key_frame;  
// }  

typedef struct MediacodecContext{
	jbyteArray dec_buffer_array;
    jbyteArray dec_yuv_array;
	
	jclass class_Codec;
	jclass class_H264Decoder;
	jclass class_HH264Decoder;
	jclass class_H264Utils;
	jclass class_Integer;
	jclass class_CodecCapabilities;
	
	jmethodID methodID_HH264Decoder_constructor;
	jmethodID methodID_HH264Decoder_config;
	jmethodID methodID_HH264Decoder_getConfig;
	jmethodID methodID_HH264Decoder_open;
	jmethodID methodID_HH264Decoder_decode;
	jmethodID methodID_HH264Decoder_getErrorCode;
	jmethodID methodID_HH264Decoder_close;
	jmethodID methodID_H264Utils_ffAvcFindStartcode;
	jmethodID methodID_Integer_intValue;
	jfieldID fieldID_Codec_ERROR_CODE_INPUT_BUFFER_FAILURE;
	jfieldID fieldID_H264Decoder_KEY_CONFIG_WIDTH;
	jfieldID fieldID_H264Decoder_KEY_CONFIG_HEIGHT;
	jfieldID fieldID_HH264Decoder_KEY_COLOR_FORMAT;
	jfieldID fieldID_HH264Decoder_KEY_MIME;
	jfieldID fieldID_CodecCapabilities_COLOR_FormatYUV420Planar;
	jfieldID fieldID_CodecCapabilities_COLOR_FormatYUV420SemiPlanar;
	
	jint ERROR_CODE_INPUT_BUFFER_FAILURE;
	jobject KEY_CONFIG_WIDTH;
	jobject KEY_CONFIG_HEIGHT;
	jobject KEY_COLOR_FORMAT;
	jint COLOR_FormatYUV420Planar;
	jint COLOR_FormatYUV420SemiPlanar;
	
	jobject object_decoder;
}MediacodecContext;
void mediacodec_decode_video(JNIEnv* env, MediacodecContext* mediacodecContext, AVPacket *packet, AVFrame *pFrame, int *got_picture);
void mediacodec_decode_video2(MediaCodecDecoder* decoder, AVPacket *packet, AVFrame *pFrame, int *got_picture);

int main(JNIEnv* env,int argc, char* argv[])
{
	//atexit(SDL_Quit);
	//atexit(clearBreakEvent);
	if(argc == 5)
	{
		LOGI("argc:%d,argv[0]:%s,argv[1]:%s,argv[2]:%s,argv[3]:%s,argv[4]:%s",argc,argv[0],argv[1],argv[2],argv[3],argv[4]);

		SDL_Window *screen;
		SDL_Renderer* sdlRenderer;
		SDL_Texture* sdlTexture;
		FILE *fp;
		SDL_Thread *refresh_thread;
		SDL_Event event;
		int frameConut = 0;

		pixel_w = atoi(argv[1]);
		pixel_h = atoi(argv[2]);
		LOGI("pixel_w:%d,pixel_h:%d",pixel_w,pixel_h);

		unsigned char *buffer = (unsigned char*)malloc(sizeof(unsigned char*) * pixel_w * pixel_h * bpp / 8);
		
		jclass objcetClass = (*env)->FindClass(env, SDL_JAVA_PREFIX_CLASS);
		jmethodID methodID_setProgressRate = (*env)->GetStaticMethodID(env, objcetClass, "setProgressRate", "(I)V");
		jmethodID methodID_setProgressRateFull = (*env)->GetStaticMethodID(env, objcetClass, "setProgressRateFull", "()V");
		jmethodID methodID_initOrientation = (*env)->GetStaticMethodID(env, objcetClass, "initOrientation", "()V");
		
		if(SDL_Init(SDL_INIT_VIDEO))
		{
			LOGE("SDL_Init failed: %s", SDL_GetError());
			clearBreakEvent();	
			SDL_Quit();
			return -1;
		}

//SDL 2.0 Support for multiple windows
		screen = SDL_CreateWindow("Simplest Video Play SDL2", SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
		                          screen_w, screen_h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

		if(screen == NULL)
		{
			LOGE("SDL_CreateWindow failed:  %s", SDL_GetError());
			clearBreakEvent();	
			SDL_Quit();
			return -2;
		}

		//SDL_GetWindowSize(screen,&screen_w,&screen_h);
		//LOGI("screen_w:%d  screen_h:%d",screen_w,screen_h);
		if(pixel_w > pixel_h){
			(*env)->CallStaticVoidMethod(env, objcetClass, methodID_initOrientation);
		}

		sdlRenderer = SDL_CreateRenderer(screen, -1, 0);
		if (sdlRenderer == NULL)
		{
			LOGE("SDL_CreateRenderer failed:  %s", SDL_GetError());
			clearBreakEvent();	
			SDL_Quit();
			return -3;
		}

		Uint32 pixformat=0;
//IYUV: Y + U + V  (3 planes)
//YV12: Y + V + U  (3 planes)
		pixformat= SDL_PIXELFORMAT_IYUV;
		int pixel_type = atoi(argv[3]);
		switch(pixel_type){
			case 0: pixformat = SDL_PIXELFORMAT_IYUV; break;
			case 1: pixformat = SDL_PIXELFORMAT_YV12; break;
			case 2: pixformat = SDL_PIXELFORMAT_YUY2; break;
			case 3: pixformat = SDL_PIXELFORMAT_UYVY; break;
			case 4: pixformat = SDL_PIXELFORMAT_YVYU; break;
			case 5: pixformat = SDL_PIXELFORMAT_NV12; break;
			case 6: pixformat = SDL_PIXELFORMAT_NV21; break;
			default:pixformat = SDL_PIXELFORMAT_IYUV; break;
		}
		LOGI("pixel_type:%d,pixformat:%d",pixel_type,pixformat);

		sdlTexture = SDL_CreateTexture(sdlRenderer, pixformat, SDL_TEXTUREACCESS_STREAMING, pixel_w, pixel_h);

//FIX: If window is resize
		angle=0;
		zoom=0;
		updateSdlRect(zoom);

		fp=fopen(argv[0],"rb+");

		if(fp==NULL)
		{
			LOGE("cannot open this file:  %s", SDL_GetError());
			clearBreakEvent();	
			SDL_Quit();
			return -4;
		}

		int fps = atoi(argv[4]);
		sdl_delay = (int)(1000.0/fps);
        LOGI("sdl_delay: %d",sdl_delay);

		refresh_thread = SDL_CreateThread(refresh_video,NULL,NULL);
		while(1)
		{
//Wait
			SDL_WaitEvent(&event);
			if(event.type==REFRESH_EVENT)
			{
				if (thread_forward == 1)
				{
					fseek(fp,(pixel_w * pixel_h * bpp / 8) * skipFrame,SEEK_CUR);
					thread_forward = 0;
					frameConut = frameConut + skipFrame;
					LOGI("前进%5lld帧~~~~",skipFrame);
				}
				if (thread_backward == 1)
				{
					fseek(fp,(pixel_w * pixel_h * bpp / 8) * skipFrame,SEEK_CUR);
					thread_backward = 0;
					frameConut = frameConut + skipFrame;
					if(frameConut<0)frameConut=0;
					LOGI("后退%5lld帧~~~~",skipFrame*-1);
				}
				if(thread_Seek == 1)
				{
					fseek(fp,(pixel_w * pixel_h * bpp / 8) * seekFrame,SEEK_SET);
					thread_Seek = 0;
					frameConut = seekFrame;
					if(frameConut<0)frameConut=0;
					LOGI("跳转到%5lld帧~~~~",seekFrame);
				}
				if (fread(buffer, 1, pixel_w * pixel_h * bpp / 8, fp) !=
				        pixel_w * pixel_h * bpp / 8)
				{
//Loop
//fseek(fp, 0, SEEK_SET);
//fread(buffer, 1, pixel_w*pixel_h*bpp/8, fp);
					thread_exit = 1;
				}
				SDL_UpdateTexture(sdlTexture, NULL, buffer, pixel_w);
				SDL_RenderClear(sdlRenderer);
				SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, &sdlRect);
				SDL_RenderPresent(sdlRenderer);

				frameConut++;
				if(thread_exit == 0)
					(*env)->CallStaticVoidMethod(env, objcetClass, methodID_setProgressRate,frameConut);

			}
			else if(event.type==SDL_QUIT)
			{
				thread_exit=1;
			}
			else if(event.type==BREAK_EVENT)
			{
				LOGI("SDL_WaitEvent(BREAK_EVENT)");
				if(thread_exit == 1)
				{
					(*env)->CallStaticVoidMethod(env, objcetClass, methodID_setProgressRateFull);
				}
				break;
			}
		}
		
		LOGI("SDL_Quit");
		thread_exit = 0;
		thread_back = 0;
		
		free(buffer);
		clearBreakEvent();	
		SDL_Quit();
		return 0;
	}
	else
	{
		LOGI("argc:%d,argv[0]:%s",argc,argv[0]);
		AVFormatContext	*pFormatCtx;
		AVDictionary *options;
		int				i, videoindex;
		AVCodecContext	*pCodecCtx;
		AVCodec			*pCodec;
		AVFrame	*pFrame,*pFrameYUV,*pFrameRotate;
		uint8_t *out_buffer1,*out_buffer2;
		AVPacket *packet;
		AVBitStreamFilterContext* bsfc;
		int ret, got_picture;
		long long frameConut = 0;
		long long current_dts = 0;
		long long forward_dts = 0;
		int forwardOffset = 0;

		//RGB8888
		int depth = 4*8;//bits
		int rmask = 0x00FF0000;
		int gmask = 0x0000FF00;
		int bmask = 0x000000FF;
		int amask = 0x00000000;
	
		SDL_Window *screen;
		SDL_Renderer* sdlRenderer;
		SDL_Texture* sdlTexture_ffmpeg;
		SDL_Texture* sdlTexture_mediacodec;
		SDL_Event event;

		struct SwsContext *img_convert_ctx;

		jclass objcetClass = (*env)->FindClass(env, SDL_JAVA_PREFIX_CLASS);
		jmethodID methodID_setProgressRateFull = (*env)->GetStaticMethodID(env, objcetClass, "setProgressRateFull", "()V");
		jmethodID methodID_setProgressDTS = (*env)->GetStaticMethodID(env, objcetClass, "setProgressDTS", "(J)V");
		jmethodID methodID_setProgressDuration = (*env)->GetStaticMethodID(env, objcetClass, "setProgressDuration", "(J)V");
		jmethodID methodID_showIFrameDTS = (*env)->GetStaticMethodID(env, objcetClass, "showIFrameDTS", "(JI)V");
		jmethodID methodID_initOrientation = (*env)->GetStaticMethodID(env, objcetClass, "initOrientation", "()V");
		jmethodID methodID_hideLoading = (*env)->GetStaticMethodID(env, objcetClass, "hideLoading", "()V");
		jmethodID methodID_changeCodec = (*env)->GetStaticMethodID(env, objcetClass, "changeCodec", "(Ljava/lang/String;)V");
		jfieldID fieldID_time_base = (*env)->GetStaticFieldID(env, objcetClass, "time_base", "D");

//char filepath[]="sintel264.h264";
//char filepath[]="rtsp://192.168.133.145:8554/111";

		av_register_all();
		avcodec_register_all();
		avformat_network_init();
		pFormatCtx = avformat_alloc_context();
		// pFormatCtx->interrupt_callback.callback = CheckInterrupt;//超时回调
		// pFormatCtx->interrupt_callback.opaque = this;

		options = NULL;
		av_dict_set(&options,"rtsp_transport","tcp",0);
		av_dict_set(&options,"stimeout","5000000",0);//超时5秒
		
		if(avformat_open_input(&pFormatCtx,argv[0],NULL,&options)!=0)
		{
			LOGE("Couldn't open input stream.\n");
			clearBreakEvent();	
			SDL_Quit();
			return -5;
		}
		if(avformat_find_stream_info(pFormatCtx,NULL)<0)
		{
			LOGE("Couldn't find stream information.\n");
			clearBreakEvent();	
			SDL_Quit();
			return -6;
		}

		videoindex=-1;
		for(i=0; i<pFormatCtx->nb_streams; i++)
		if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO){
			videoindex=i;
			break;
		}
		if(videoindex==-1){
			LOGE("Didn't find a video stream.\n");
			clearBreakEvent();	
			SDL_Quit();
			return -7;
		}
		pCodecCtx=pFormatCtx->streams[videoindex]->codec;
		pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
		if(pCodec==NULL){
			LOGE("Codec not found.\n");
			clearBreakEvent();	
			SDL_Quit();
			return -8;
		}
		
		if(avcodec_open2(pCodecCtx, pCodec,NULL)<0){
			LOGE("Could not open codec.\n");
			clearBreakEvent();	
			SDL_Quit();
			return -9;
		}
		
		pixel_w = pCodecCtx->width;
		pixel_h = pCodecCtx->height;
		LOGI("pixel_w:%d  pixel_h:%d",pixel_w,pixel_h);
		if(pixel_w == 0 || pixel_h == 0){
			LOGE("video stream error.\n");
			clearBreakEvent();	
			SDL_Quit();
			return -10;
		}

		LOGI("format name: %s",pFormatCtx->iformat->name);

		if(strcmp("avi",pFormatCtx->iformat->name) == 0 || strcmp("h264",pFormatCtx->iformat->name) == 0){
			(*env)->SetStaticDoubleField(env, objcetClass, fieldID_time_base, 1/av_q2d(pFormatCtx->streams[videoindex]->r_frame_rate));
            LOGI("frame_rate: %lf",av_q2d(pFormatCtx->streams[videoindex]->r_frame_rate));
		}
		else{
			(*env)->SetStaticDoubleField(env, objcetClass, fieldID_time_base, av_q2d(pFormatCtx ->streams[videoindex]->time_base));
		    LOGI("time_base: %lf",av_q2d(pFormatCtx ->streams[videoindex]->time_base));
        }

		sdl_delay = (int)(1000.0/av_q2d(pFormatCtx->streams[videoindex]->r_frame_rate));
		LOGI("sdl_delay: %d",sdl_delay);
//		double maxTime;
//		if(pFormatCtx ->streams[videoindex]->duration<0){
//			maxTime = (double)pFormatCtx ->duration/1000000;
//		}
//		else{
//			maxTime = (double)pFormatCtx ->streams[videoindex]->duration
//				 * av_q2d(pFormatCtx ->streams[videoindex]->time_base);
//		}
//		(*env)->CallStaticVoidMethod(env, objcetClass, methodID_setProgressMaxRate,(int)maxTime);
		long long duration;
		if(pFormatCtx ->streams[videoindex]->duration<0){
			duration = (long long)((double)pFormatCtx ->duration / 1000000 / av_q2d(pFormatCtx ->streams[videoindex]->time_base)) ;
		}
		else{
			duration = pFormatCtx ->streams[videoindex]->duration;
		}
		if(strcmp("h264",pFormatCtx->iformat->name) == 0)duration = 0;
		(*env)->CallStaticVoidMethod(env, objcetClass, methodID_setProgressDuration,duration);

		pFrame=av_frame_alloc();
		pFrameYUV=av_frame_alloc();
		out_buffer1=(uint8_t *)av_malloc(avpicture_get_size(PIX_FMT_YUV420P, pCodecCtx->width+16, pCodecCtx->height+16));
		avpicture_fill((AVPicture *)pFrameYUV, out_buffer1, PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height);
		LOGI("out_buffer1=%d",avpicture_get_size(PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height));
		
		pFrameRotate = av_frame_alloc(); 
		out_buffer2=(uint8_t *)av_malloc(avpicture_get_size(PIX_FMT_YUV420P, pCodecCtx->height+16, pCodecCtx->width+16));
		avpicture_fill((AVPicture *)pFrameRotate, out_buffer2, PIX_FMT_YUV420P, pCodecCtx->height, pCodecCtx->width);
		
		img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
		                                 pCodecCtx->width, pCodecCtx->height, PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);

		bsfc = av_bitstream_filter_init("h264_mp4toannexb");
		
//------------Mediacodec init----------------		
#if MEDIACODEC_ANDROID
		MediacodecContext mediacodecContext;
		
		mediacodecContext.dec_buffer_array = (*env)->NewByteArray(env, MEDIACODEC_DEC_BUFFER_ARRAY_SIZE);	
		mediacodecContext.dec_yuv_array = (*env)->NewByteArray(env, MEDIACODEC_DEC_YUV_ARRAY_SIZE);	
		
		mediacodecContext.class_Codec = (*env)->FindClass(env, "com/example/ffmpegsdlplayer/mediacodec/Codec");
		mediacodecContext.class_H264Decoder = (*env)->FindClass(env, "com/example/ffmpegsdlplayer/mediacodec/H264Decoder");
		mediacodecContext.class_HH264Decoder = (*env)->FindClass(env, "com/example/ffmpegsdlplayer/mediacodec/HH264Decoder");
		mediacodecContext.class_H264Utils = (*env)->FindClass(env, "com/example/ffmpegsdlplayer/mediacodec/H264Utils");
		mediacodecContext.class_Integer = (*env)->FindClass(env, "java/lang/Integer");
		mediacodecContext.class_CodecCapabilities = (*env)->FindClass(env,"android/media/MediaCodecInfo$CodecCapabilities");
		
		mediacodecContext.methodID_HH264Decoder_constructor = (*env)->GetMethodID(env,mediacodecContext.class_HH264Decoder,"<init>","()V");
		mediacodecContext.methodID_HH264Decoder_config = (*env)->GetMethodID(env,mediacodecContext.class_HH264Decoder,"config","(Ljava/lang/String;Ljava/lang/Object;)V");
		mediacodecContext.methodID_HH264Decoder_getConfig = (*env)->GetMethodID(env,mediacodecContext.class_HH264Decoder,"getConfig","(Ljava/lang/String;)Ljava/lang/Object;");
		mediacodecContext.methodID_HH264Decoder_open = (*env)->GetMethodID(env,mediacodecContext.class_HH264Decoder,"open","()V");
		mediacodecContext.methodID_HH264Decoder_decode = (*env)->GetMethodID(env,mediacodecContext.class_HH264Decoder,"decode","([BI[BI)I");
		mediacodecContext.methodID_HH264Decoder_getErrorCode = (*env)->GetMethodID(env,mediacodecContext.class_HH264Decoder,"getErrorCode","()I");
		mediacodecContext.methodID_HH264Decoder_close = (*env)->GetMethodID(env,mediacodecContext.class_HH264Decoder,"close","()V");
		mediacodecContext.methodID_H264Utils_ffAvcFindStartcode = (*env)->GetStaticMethodID(env,mediacodecContext.class_H264Utils,"ffAvcFindStartcode","([BII)I");
		mediacodecContext.methodID_Integer_intValue = (*env)->GetMethodID(env,mediacodecContext.class_Integer,"intValue","()I");
		mediacodecContext.fieldID_Codec_ERROR_CODE_INPUT_BUFFER_FAILURE = (*env)->GetStaticFieldID(env, mediacodecContext.class_Codec, "ERROR_CODE_INPUT_BUFFER_FAILURE", "I");
		mediacodecContext.fieldID_H264Decoder_KEY_CONFIG_WIDTH = (*env)->GetStaticFieldID(env, mediacodecContext.class_H264Decoder, "KEY_CONFIG_WIDTH", "Ljava/lang/String;");
		mediacodecContext.fieldID_H264Decoder_KEY_CONFIG_HEIGHT = (*env)->GetStaticFieldID(env, mediacodecContext.class_H264Decoder, "KEY_CONFIG_HEIGHT", "Ljava/lang/String;");
		mediacodecContext.fieldID_HH264Decoder_KEY_COLOR_FORMAT = (*env)->GetStaticFieldID(env, mediacodecContext.class_HH264Decoder, "KEY_COLOR_FORMAT", "Ljava/lang/String;");
		mediacodecContext.fieldID_HH264Decoder_KEY_MIME = (*env)->GetStaticFieldID(env, mediacodecContext.class_HH264Decoder, "KEY_MIME", "Ljava/lang/String;");
		mediacodecContext.fieldID_CodecCapabilities_COLOR_FormatYUV420Planar = (*env)->GetStaticFieldID(env,  mediacodecContext.class_CodecCapabilities, "COLOR_FormatYUV420Planar", "I");
		mediacodecContext.fieldID_CodecCapabilities_COLOR_FormatYUV420SemiPlanar = (*env)->GetStaticFieldID(env,  mediacodecContext.class_CodecCapabilities, "COLOR_FormatYUV420SemiPlanar", "I");
		
		mediacodecContext.ERROR_CODE_INPUT_BUFFER_FAILURE = (*env)->GetStaticIntField(env, mediacodecContext.class_Codec, mediacodecContext.fieldID_Codec_ERROR_CODE_INPUT_BUFFER_FAILURE);
		mediacodecContext.KEY_CONFIG_WIDTH = (*env)->GetStaticObjectField(env, mediacodecContext.class_H264Decoder, mediacodecContext.fieldID_H264Decoder_KEY_CONFIG_WIDTH);
		mediacodecContext.KEY_CONFIG_HEIGHT = (*env)->GetStaticObjectField(env, mediacodecContext.class_H264Decoder, mediacodecContext.fieldID_H264Decoder_KEY_CONFIG_HEIGHT);
		mediacodecContext.KEY_COLOR_FORMAT = (*env)->GetStaticObjectField(env, mediacodecContext.class_HH264Decoder, mediacodecContext.fieldID_HH264Decoder_KEY_COLOR_FORMAT);
		mediacodecContext.COLOR_FormatYUV420Planar = (*env)->GetStaticIntField(env, mediacodecContext.class_CodecCapabilities, mediacodecContext.fieldID_CodecCapabilities_COLOR_FormatYUV420Planar);
		mediacodecContext.COLOR_FormatYUV420SemiPlanar = (*env)->GetStaticIntField(env, mediacodecContext.class_CodecCapabilities, mediacodecContext.fieldID_CodecCapabilities_COLOR_FormatYUV420SemiPlanar);
		
		mediacodecContext.object_decoder = (*env)->NewObject(env, mediacodecContext.class_HH264Decoder, mediacodecContext.methodID_HH264Decoder_constructor);
		(*env)->CallVoidMethod(env, mediacodecContext.object_decoder, mediacodecContext.methodID_HH264Decoder_open);
#else		
		
		MediaCodecDecoder* mediacodec_decoder = mediacodec_decoder_alloc3();
		mediacodec_decoder_open(mediacodec_decoder);
#endif
//------------Mediacadec----------------------

//------------SDL-----------------------------
		if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
		{
			LOGE( "Could not initialize SDL - %s\n", SDL_GetError());
			clearBreakEvent();	
			SDL_Quit();
			return -1;
		}

		screen = SDL_CreateWindow("Simplest ffmpeg player's Window", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		                          screen_w, screen_h,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);

		if(!screen)
		{
			LOGE("SDL: could not create window - exiting:%s\n",SDL_GetError());
			clearBreakEvent();	
			SDL_Quit();
			return -2;
		}

		//SDL_GetWindowSize(screen,&screen_w,&screen_h);
		//LOGI("screen_w:%d  screen_h:%d",screen_w,screen_h);
		
		AVDictionaryEntry *tag = NULL;
		tag = av_dict_get(pFormatCtx ->streams[videoindex]->metadata,"rotate",tag,0);
		if(tag == NULL){
			angle = 0;
		}
		else{
			angle = atoi(tag->value);
			angle %= 360;
		}

		LOGI("视频旋转了 %d 度~~~~",angle);
		if(pixel_w > pixel_h && angle == 0){
			(*env)->CallStaticVoidMethod(env, objcetClass, methodID_initOrientation);
		}

		sdlRenderer = SDL_CreateRenderer(screen, -1, 0);
		if (sdlRenderer == NULL)
		{
			LOGE("SDL_CreateRenderer failed:  %s", SDL_GetError());
			clearBreakEvent();	
			SDL_Quit();
			return -3;
		}
//IYUV: Y + U + V  (3 planes)
//YV12: Y + V + U  (3 planes)

		sdlTexture_ffmpeg = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,pCodecCtx->width,pCodecCtx->height);
		sdlTexture_mediacodec = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_NV12, SDL_TEXTUREACCESS_STREAMING,pCodecCtx->width,pCodecCtx->height);
		// if(angle == 90 || angle == 270){
			// sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,pCodecCtx->height,pCodecCtx->width);
		// }
		// else{
			// sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,pCodecCtx->width,pCodecCtx->height);		
		// }
		
		zoom=0;
		updateSdlRect(zoom);

		packet=(AVPacket *)av_malloc(sizeof(AVPacket));

		SDL_Thread *refresh_thread = SDL_CreateThread(refresh_video,NULL,NULL);
//------------SDL End------------		

		while(1)//Event Loop
		{
			int status  = SDL_WaitEvent(&event);//Wait
			// LOGI("SDL_WaitEvent() event.type=%d",event.type);
			if(event.type==REFRESH_EVENT)
			{
				if (thread_forward == -1)
				{
					long long skip_dts = current_dts + (long long)((double)skipFrame / av_q2d(pFormatCtx ->streams[videoindex]->time_base));
					av_seek_frame(pFormatCtx,videoindex,skip_dts,AVSEEK_FLAG_BACKWARD);
					LOGI("%lld######11#####%lld",current_dts,skip_dts);
				}
				if (thread_forward == 1)
				{
					forward_dts = current_dts;
					long long skip_dts = current_dts + (long long)((double)skipFrame / av_q2d(pFormatCtx ->streams[videoindex]->time_base));
					thread_forward = -1;
					LOGI("%lld######22#####%lld",current_dts,skip_dts);
					av_seek_frame(pFormatCtx,videoindex,skip_dts,AVSEEK_FLAG_BACKWARD);

					if(skip_dts >= duration){
						av_seek_frame(pFormatCtx,videoindex,skip_dts,AVSEEK_FLAG_ANY);
						thread_forward = -2;
					}
				}
				if (thread_backward == 1)
				{
					long long skip_dts = current_dts + (long long)((double)skipFrame / av_q2d(pFormatCtx ->streams[videoindex]->time_base));
					av_seek_frame(pFormatCtx,videoindex,skip_dts,AVSEEK_FLAG_BACKWARD);
					thread_backward = 0;
				}
				if(thread_Seek == 1)
				{
					LOGI("%lld######33#####",seekFrame);
					av_seek_frame(pFormatCtx,videoindex,seekFrame,AVSEEK_FLAG_BACKWARD);
					thread_Seek = 0;
				}
				while (1)
				{
					if(av_read_frame(pFormatCtx, packet)>=0)
					{
						if(packet->stream_index==videoindex)
						{
							frameConut++;
							if(thread_exit != 1){
								if(is_Locating_I == 1){
									if(thread_forward == -2){
										(*env)->CallStaticVoidMethod(env, objcetClass, methodID_showIFrameDTS, packet->dts,-1);
										thread_forward = 0;
										is_Locating_I = 0;
									}
									else if(thread_forward == -1){
										if(packet->dts < forward_dts){
											forwardOffset = 1;
											(*env)->CallStaticVoidMethod(env, objcetClass, methodID_showIFrameDTS, packet->dts,forwardOffset);
                                        }
										else if(packet->dts == forward_dts){
											forwardOffset++;
											(*env)->CallStaticVoidMethod(env, objcetClass, methodID_showIFrameDTS, packet->dts,forwardOffset);
										}
										else{
											(*env)->CallStaticVoidMethod(env, objcetClass, methodID_showIFrameDTS, packet->dts,0);
											thread_forward = 0;
											is_Locating_I = 0;
										}
									}
									else{
										(*env)->CallStaticVoidMethod(env, objcetClass, methodID_showIFrameDTS, packet->dts,0);
										is_Locating_I = 0;
									};
								}

								//LOGI("正在解码第%05lld帧~~~~",frameConut);
								if(frameConut == 1){
									(*env)->CallStaticVoidMethod(env, objcetClass, methodID_hideLoading);
								}
								if(!strcmp("h264",pFormatCtx->iformat->name)){
									(*env)->CallStaticVoidMethod(env, objcetClass, methodID_setProgressDTS,frameConut);
								}
								else{
									current_dts = packet->dts;
									(*env)->CallStaticVoidMethod(env, objcetClass, methodID_setProgressDTS,packet->dts);
								}
							}
							else{
								thread_forward = 0;
								is_Locating_I = 0;
							}
							break;
						}
						else
						{
							av_free_packet(packet);
						}
					}
					else
					{
//Exit Thread
						thread_exit=1;
						break;
					}
				}
				
				if(strcmp("h264", pCodec->name) && thread_codec_type == 1){//非h264编码 且 选择硬件解码
					(*env)->CallStaticVoidMethod(env, objcetClass, methodID_changeCodec, (*env)->NewStringUTF(env,pCodec->name));
					thread_codec_type = 0;
				}
				
				LOGI("thread_codec_type = %d",thread_codec_type);
				
				if(thread_codec_type == 0){
					ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);

					if(ret < 0)
					{
						LOGE("Decode Error.\n");
						continue;
						//return -11;
					}
					if(got_picture)
					{
						sws_scale(img_convert_ctx, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameYUV->data, pFrameYUV->linesize);
						pFrameYUV->width = pFrame->width;
						pFrameYUV->height = pFrame->height;
//SDL----------------------------------------------------------------------------------------
						// if(angle == 90){
							// frame_rotate_90(pFrameYUV,pFrameRotate);
							// SDL_UpdateTexture( sdlTexture, NULL, pFrameRotate->data[0], pFrameRotate->linesize[0] );
							// SDL_RenderClear( sdlRenderer );
							// SDL_RenderCopy( sdlRenderer, sdlTexture, NULL, &sdlRect);
							// SDL_RenderPresent( sdlRenderer );
						// }else if(angle == 180){
							// frame_rotate_180(pFrameYUV,pFrameRotate);
							// SDL_UpdateTexture( sdlTexture, NULL, pFrameRotate->data[0], pFrameRotate->linesize[0] );
							// SDL_RenderClear( sdlRenderer );
							// SDL_RenderCopy( sdlRenderer, sdlTexture, NULL, &sdlRect);
							// SDL_RenderPresent( sdlRenderer );
						// }else if(angle == 270){
							// frame_rotate_270(pFrameYUV,pFrameRotate);
							// SDL_UpdateTexture( sdlTexture, NULL, pFrameRotate->data[0], pFrameRotate->linesize[0] );
							// SDL_RenderClear( sdlRenderer );
							// SDL_RenderCopy( sdlRenderer, sdlTexture, NULL, &sdlRect);
							// SDL_RenderPresent( sdlRenderer );
						// }else{
							// SDL_UpdateTexture( sdlTexture, NULL, pFrameYUV->data[0], pFrameYUV->linesize[0] );
							// SDL_RenderClear( sdlRenderer );
							// SDL_RenderCopy( sdlRenderer, sdlTexture, NULL, &sdlRect);
							// SDL_RenderPresent( sdlRenderer );
						// }
						if(angle == 0){
							SDL_UpdateTexture( sdlTexture_ffmpeg, NULL, pFrameYUV->data[0], pFrameYUV->linesize[0] );
							SDL_RenderClear( sdlRenderer );
							SDL_RenderCopy( sdlRenderer, sdlTexture_ffmpeg, NULL, &sdlRect);
							SDL_RenderPresent( sdlRenderer );
						}else{
							SDL_UpdateTexture( sdlTexture_ffmpeg, NULL, pFrameYUV->data[0], pFrameYUV->linesize[0] );
							SDL_RenderClear( sdlRenderer );
							SDL_RenderCopyEx( sdlRenderer, sdlTexture_ffmpeg, NULL, &sdlRect, angle, &sdlPoint, SDL_FLIP_NONE);
							SDL_RenderPresent( sdlRenderer );
						}

						if(thread_picture == 1){
							thread_picture = 0;
							SDL_Surface* savePicture = SDL_CreateRGBSurface(0,sdlRect.w,sdlRect.h,depth,rmask,gmask,bmask,amask);
							SDL_RenderReadPixels(sdlRenderer, &sdlRect, SDL_PIXELFORMAT_RGB888, savePicture->pixels, sdlRect.w*4 );
							SDL_SaveBMP(savePicture,"/storage/emulated/0/000.bmp");
						}
//SDL End------------------------------------------------------------------------------------
					}
				}
				else if(thread_codec_type == 1){
					av_bitstream_filter_filter(bsfc, pCodecCtx, NULL, &packet->data, &packet->size, packet->data, packet->size, 0);
					
					if(packet->size > 4){
						int nalu_type = *(packet->data + 4) & 0x1F;
						LOGI("[DTS:%lld]:nalu_first=%0X\t nalu_type=%d", packet->dts, *(packet->data + 4), nalu_type);
					}
					
					// {
						// static FILE * fin_rtsp = NULL;
						// if(!fin_rtsp)
						// {
							// fin_rtsp = fopen("/sdcard/rtsp.h264","wb");
						// }
						// if(fin_rtsp)
						// {
							// fwrite(whole_frame, 1, whole_frame_len, fin_rtsp);
							// fflush(fin_rtsp);
						// }
					// }
					
#if MEDIACODEC_ANDROID
					mediacodec_decode_video(env, &mediacodecContext, packet, pFrameYUV, &got_picture);
#else
					mediacodec_decode_video2(mediacodec_decoder, packet, pFrameYUV, &got_picture);
#endif
					if(got_picture)
					{
//SDL----------------------------------------------------------------------------------------
						// if(angle == 90){
							// frame_rotate_90(pFrameYUV,pFrameRotate);
							// SDL_UpdateTexture( sdlTexture, NULL, pFrameRotate->data[0], pFrameRotate->linesize[0] );
							// SDL_RenderClear( sdlRenderer );
							// SDL_RenderCopy( sdlRenderer, sdlTexture, NULL, &sdlRect);
							// SDL_RenderPresent( sdlRenderer );
						// }else if(angle == 180){
							// frame_rotate_180(pFrameYUV,pFrameRotate);
							// SDL_UpdateTexture( sdlTexture, NULL, pFrameRotate->data[0], pFrameRotate->linesize[0] );
							// SDL_RenderClear( sdlRenderer );
							// SDL_RenderCopy( sdlRenderer, sdlTexture, NULL, &sdlRect);
							// SDL_RenderPresent( sdlRenderer );
						// }else if(angle == 270){
							// frame_rotate_270(pFrameYUV,pFrameRotate);
							// SDL_UpdateTexture( sdlTexture, NULL, pFrameRotate->data[0], pFrameRotate->linesize[0] );
							// SDL_RenderClear( sdlRenderer );
							// SDL_RenderCopy( sdlRenderer, sdlTexture, NULL, &sdlRect);
							// SDL_RenderPresent( sdlRenderer );
						// }else{
							// SDL_UpdateTexture( sdlTexture, NULL, pFrameYUV->data[0], pFrameYUV->linesize[0] );
							// SDL_RenderClear( sdlRenderer );
							// SDL_RenderCopy( sdlRenderer, sdlTexture, NULL, &sdlRect);
							// SDL_RenderPresent( sdlRenderer );
						// }
						if(angle == 0){
							SDL_UpdateTexture( sdlTexture_mediacodec, NULL, pFrameYUV->data[0], pFrameYUV->linesize[0] );
							SDL_RenderClear( sdlRenderer );
							SDL_RenderCopy( sdlRenderer, sdlTexture_mediacodec, NULL, &sdlRect);
							SDL_RenderPresent( sdlRenderer );
						}else{
							SDL_UpdateTexture( sdlTexture_mediacodec, NULL, pFrameYUV->data[0], pFrameYUV->linesize[0] );
							SDL_RenderClear( sdlRenderer );
							SDL_RenderCopyEx( sdlRenderer, sdlTexture_mediacodec, NULL, &sdlRect, angle, &sdlPoint, SDL_FLIP_NONE);
							SDL_RenderPresent( sdlRenderer );
						}

						if(thread_picture == 1){
							thread_picture = 0;
							SDL_Surface* savePicture = SDL_CreateRGBSurface(0,sdlRect.w,sdlRect.h,depth,rmask,gmask,bmask,amask);
							SDL_RenderReadPixels(sdlRenderer, &sdlRect, SDL_PIXELFORMAT_RGB888, savePicture->pixels, sdlRect.w*4 );
							SDL_SaveBMP(savePicture,"/storage/emulated/0/000.bmp");
						}
//SDL End------------------------------------------------------------------------------------
					}
				}
				av_free_packet(packet);
			}
			else if(event.type==SDL_QUIT)
			{
				thread_exit=1;
			}
			else if(event.type==BREAK_EVENT)
			{
				LOGI("SDL_WaitEvent(BREAK_EVENT)");
				if(thread_exit==1)
				{
					(*env)->CallStaticVoidMethod(env, objcetClass, methodID_setProgressRateFull);
				}
				break;
			}
		}

		LOGI("SDL_Quit");
		sws_freeContext(img_convert_ctx);
		av_frame_free(&pFrameYUV);
		av_frame_free(&pFrame);
		av_frame_free(&pFrameRotate);
		av_free_packet(packet);
		avcodec_close(pCodecCtx);
		avformat_close_input(&pFormatCtx);
		av_dict_free(&options);
#if MEDIACODEC_ANDROID		
		(*env)->DeleteLocalRef(env, mediacodecContext.dec_yuv_array);
		(*env)->DeleteLocalRef(env, mediacodecContext.dec_buffer_array);
		
		(*env)->DeleteLocalRef(env, mediacodecContext.class_Codec);
		(*env)->DeleteLocalRef(env, mediacodecContext.class_H264Decoder);
		(*env)->DeleteLocalRef(env, mediacodecContext.class_HH264Decoder);
		(*env)->DeleteLocalRef(env, mediacodecContext.class_H264Utils);
		(*env)->DeleteLocalRef(env, mediacodecContext.class_Integer);
		(*env)->DeleteLocalRef(env, mediacodecContext.class_CodecCapabilities);
		
		(*env)->CallVoidMethod(env, mediacodecContext.object_decoder, mediacodecContext.methodID_HH264Decoder_close);
		(*env)->DeleteLocalRef(env, mediacodecContext.object_decoder);
		(*env)->DeleteLocalRef(env, mediacodecContext.KEY_CONFIG_WIDTH);
		(*env)->DeleteLocalRef(env, mediacodecContext.KEY_CONFIG_HEIGHT);
		(*env)->DeleteLocalRef(env, mediacodecContext.KEY_COLOR_FORMAT);
#else
		int status = mediacodec_decoder_close(mediacodec_decoder);
		LOGI("mediacodec_decoder_close status=%d",status);
		mediacodec_decoder_free(mediacodec_decoder);
#endif

		clearBreakEvent();
		SDL_Quit();
		return 0;
	}
}

void mediacodec_decode_video(JNIEnv* env, MediacodecContext* mediacodecContext, AVPacket *packet, AVFrame *pFrame, int *got_picture){
	uint8_t *in,*out;
	int in_len = packet->size;
	int out_len = 0;
	in = packet->data;
	out = pFrame->data[0];
	int repeat_count = 0;
	jint jindex,
		 jyuv_len,
		 jerror_code,
		 jyuv_wdith,
		 jyuv_height,
		 jyuv_pixel;
		 
	(*env)->SetByteArrayRegion(env, mediacodecContext->dec_buffer_array, 0, in_len, (jbyte*)in);
		 
	while(1){
		jyuv_len = (*env)->CallIntMethod(env,mediacodecContext->object_decoder,mediacodecContext->methodID_HH264Decoder_decode,mediacodecContext->dec_buffer_array,0,mediacodecContext->dec_yuv_array,in_len);
					
		jerror_code = (*env)->CallIntMethod(env,mediacodecContext->object_decoder,mediacodecContext->methodID_HH264Decoder_getErrorCode);
		LOGI("yuv_len:%6d\t error_code:%6d", jyuv_len,jerror_code);
		
		if(jerror_code == mediacodecContext->ERROR_CODE_INPUT_BUFFER_FAILURE){
			if(repeat_count < 5){
				repeat_count++;
				usleep(1000);
				continue;
			}
			else{
				repeat_count = 0;
			}
		}
		
		if(jyuv_len > 0){
			jobject yuv_wdith = (*env)->CallObjectMethod(env, mediacodecContext->object_decoder, mediacodecContext->methodID_HH264Decoder_getConfig, mediacodecContext->KEY_CONFIG_WIDTH);
			jobject yuv_height = (*env)->CallObjectMethod(env, mediacodecContext->object_decoder, mediacodecContext->methodID_HH264Decoder_getConfig, mediacodecContext->KEY_CONFIG_HEIGHT);
			jobject yuv_pixel = (*env)->CallObjectMethod(env, mediacodecContext->object_decoder, mediacodecContext->methodID_HH264Decoder_getConfig, mediacodecContext->KEY_COLOR_FORMAT);
			jyuv_wdith = (*env)->CallIntMethod(env, yuv_wdith, mediacodecContext->methodID_Integer_intValue);
			jyuv_height = (*env)->CallIntMethod(env, yuv_height, mediacodecContext->methodID_Integer_intValue);
			jyuv_pixel = (*env)->CallIntMethod(env, yuv_pixel, mediacodecContext->methodID_Integer_intValue);
			LOGI("W x H : %d x %d\t yuv_pixel:%6d", jyuv_wdith,jyuv_height,jyuv_pixel);
			
			(*env)->DeleteLocalRef(env,yuv_wdith);
			(*env)->DeleteLocalRef(env,yuv_height);
			(*env)->DeleteLocalRef(env,yuv_pixel);
			
			(*env)->GetByteArrayRegion(env, mediacodecContext->dec_yuv_array, 0, jyuv_len, (jbyte*)out);
			out_len = jyuv_len;
		}
		break;
	}
	
	if(out_len > 0){
		*got_picture = 1;
	}
	else{
		*got_picture = 0;
	}
}

void mediacodec_decode_video2(MediaCodecDecoder* decoder, AVPacket *packet, AVFrame *pFrame, int *got_picture){
	uint8_t *in,*out;
	int in_len = packet->size;
	int out_len = 0;
	in = packet->data;
	out = pFrame->data[0];
	int repeat_count = 0;
	int jindex,
		jyuv_len,
		jerror_code,
		jyuv_wdith,
		jyuv_height,
		jyuv_pixel;
		 
	while(1){
		jyuv_len = mediacodec_decoder_decode(decoder, in, 0, out, in_len, &jerror_code);
		LOGI("yuv_len:%6d\t error_code:%6d", jyuv_len,jerror_code);
		
		if(jerror_code == -3){
			LOGE("error_code:%d TIME_OUT:%d",jerror_code,mediacodec_decoder_getConfig_int(decoder, "timeout"));
			if(repeat_count < 3){
				repeat_count++;
				usleep(10000);
				if(mediacodec_decoder_getConfig_int(decoder, "timeout") < mediacodec_decoder_getConfig_int(decoder, "max-timeout")){
					mediacodec_decoder_setConfig_int(decoder, "timeout", mediacodec_decoder_getConfig_int(decoder, "timeout")+200);
				}
				else{
					mediacodec_decoder_setConfig_int(decoder, "timeout", mediacodec_decoder_getConfig_int(decoder, "max-timeout"));
				}
				continue;
			}
			else{
				repeat_count = 0;
			}
		}
		
		if(jerror_code <= -10000){
			LOGE("硬件编解码器损坏，请更换编解码器");
			thread_codec_type = 0;
		}
		
		if(jyuv_len > 0){
			jyuv_wdith = mediacodec_decoder_getConfig_int(decoder, "width");
			jyuv_height = mediacodec_decoder_getConfig_int(decoder, "height");;
			jyuv_pixel = mediacodec_decoder_getConfig_int(decoder, "color-format");
			LOGI("W x H : %d x %d\t yuv_pixel:%6d", jyuv_wdith,jyuv_height,jyuv_pixel);
			
			pFrame->width = jyuv_wdith;
			pFrame->height = jyuv_height;
			out_len = jyuv_len;
		}
		break;
	}
	
	if(out_len > 0){
		*got_picture = 1;
	}
	else{
		*got_picture = 0;
	}
}