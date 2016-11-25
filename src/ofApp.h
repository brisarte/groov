#pragma once

#include "ofMain.h"
#include "ofxOpenCv.h"
#include "ofxKinect.h"
#include "fft.h"

#define BUFFER_SIZE 256
#define NUM_WINDOWS 80

//Beat class
class Beat {
public:
	Beat();                //Class constructor
	void setup();              //Start beat
	void update( float dt );   //Recalculate physics
	void draw(int vertices);               //Draw beat

	float time;                //Time of living
	float lifeTime;            //Allowed lifetime
	bool live;                 //Is beat live
	float opac;
	float size;
};
void desenhaVariosPoligonos();
void desenhaPoligono(int vertices, int radius, bool rotate, bool fill);
void desenhaBrisa(int nBrisa);
void desenhaBeats(int vertices);
void desenhaDepthAlpha();
void desenhaContorno();
void desenhaCamFloresta();
void desenhaCamSereias();
void desenhaOlhoGirassol();
void desenhaOlhoIllu();

class ofApp : public ofBaseApp {
public:
	
	void setup();
	void update();
	void draw();
	void exit();
	
	void drawPointCloud();

	void getBlurImage(ofxCvGrayscaleImage &imgBlur, int indiceBlur);
	void getNearMirror(ofxCvGrayscaleImage &imgGray, int contrasteDistancia);
	
	void keyPressed(int key);
	void mouseDragged(int x, int y, int button);
	void mousePressed(int x, int y, int button);
	void mouseReleased(int x, int y, int button);
	void mouseEntered(int x, int y);
	void mouseExited(int x, int y);
	void windowResized(int w, int h);
	
	void audioReceived 	(float * input, int bufferSize, int nChannels); 

	
	bool bDrawPointCloud;
	
	int angle;

	// used for viewing the point cloud
	ofEasyCam easyCam;


private:
	
ofFbo  fboLayer[4];	//buffer para telas
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
