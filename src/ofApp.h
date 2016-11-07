#pragma once

#include "ofMain.h"
#include "ofxOpenCv.h"
#include "ofxKinect.h"
#include "fft.h"

#define BUFFER_SIZE 256
#define NUM_WINDOWS 80

class ofApp : public ofBaseApp {
public:
	
	void setup();
	void update();
	void draw();
	void exit();
	
	void drawPointCloud();

	void getNearMirror(ofxCvGrayscaleImage &imgGray, int contrasteDistancia);
	
	void keyPressed(int key);
	void mouseDragged(int x, int y, int button);
	void mousePressed(int x, int y, int button);
	void mouseReleased(int x, int y, int button);
	void mouseEntered(int x, int y);
	void mouseExited(int x, int y);
	void windowResized(int w, int h);
	
	void audioReceived 	(float * input, int bufferSize, int nChannels); 

	ofxKinect kinect;
	
	ofxCvColorImage colorImg;
	
	ofxCvGrayscaleImage grayImage; // grayscale depth image
	
	bool bDrawPointCloud;
	
	int angle;

	// used for viewing the point cloud
	ofEasyCam easyCam;

	ofVideoPlayer video;

private:
	ofFbo fbo, fbo2, fboVideo;	//buffer para telas
	
	ofShader shader; //Shader
	
	// Audio
	float * left;
	float * right;
	int 	bufferCounter;
	fft		myfft;
	
	float magnitude[BUFFER_SIZE];
	float magniView[BUFFER_SIZE];
	float phase[BUFFER_SIZE];
	float power[BUFFER_SIZE];
	
	float freq[NUM_WINDOWS][BUFFER_SIZE/2];
	float freq_phase[NUM_WINDOWS][BUFFER_SIZE/2];
};
