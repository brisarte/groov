#include "ofApp.h"

const int N = 256;		//Number of bands in spectrum
float spectrum[ N ];	//Smoothed spectrum values
float beat;
float beatMin = 8;
int vw,vh;
float beatCount = 0;
float time0;
ofPoint baricenter(0,0), baricentro(0,0);

bool DEBUGMODE = true;
//--------------------------------------------------------------
//----------------------  Particle  ----------------------------
//--------------------------------------------------------------
Beat::Beat() {
	live = false;
}

//--------------------------------------------------------------
void Beat::setup() {
	time = 0;
	lifeTime = 1.0;
	live = true;
	size = 0;
	opac = 1;
}

//--------------------------------------------------------------
void Beat::update( float dt ){
	if ( live ) {
		// Atualiza tamanho e opac
		size = ofMap(time, 0, lifeTime, 0, 1);
		opac = ofMap(time, 0, lifeTime, 1, 0);

		//Update time and check if particle should die
		time += dt;
		if ( time >= lifeTime ) {
			live = false;   //Particle is now considered as died
		}
	}
}

//--------------------------------------------------------------
void Beat::draw(){
	if ( live ) {
		//Compute color
		ofColor color = ofColor::red;
		float hue = ofMap( time, 0, lifeTime, 0, 20 );
		color.setHue( hue );
		ofSetColor( color, opac*255 );

		ofDrawCircle( vw/2, vh/2, size*vh/2 );  //Draw particle

	}
}

//--------------------------------------------------------------
void ofApp::setup() {
	ofSetLogLevel(OF_LOG_VERBOSE);
	
	// enable depth->video image calibration
	kinect.setRegistration(true);
    
	kinect.init();
	//kinect.init(true); // shows infrared instead of RGB video image
	//kinect.init(false, false); // disable video image (faster fps)
	
	kinect.open();		// opens first available kinect
	//kinect.open(1);	// open a kinect by id, starting with 0 (sorted by serial # lexicographically))
	//kinect.open("A00362A08602047A");	// open a kinect using it's unique serial #
	
	
	colorImg.allocate(kinect.width, kinect.height);
	grayImage.allocate(kinect.width, kinect.height);
	blurImage.allocate(kinect.width, kinect.height);
	
	baricentro.set( blurImage.width/2, blurImage.height/2 );
	
	ofSetFrameRate(60);
	
	// zero the tilt on startup
	angle = 0;
	kinect.setCameraTiltAngle(angle);
	
	// start from the front
	bDrawPointCloud = false;

	//Load shader
	shader.load( "vertexdummy.c", "kinectshader.c" );
	shaderInvert.load( "vertexdummy.c", "invertshader.c" );

	//Carrega o vídeo
	video.load("amazonia.mp4");
	video.play();

	fbo.allocate(kinect.width, kinect.height);
	fboVideo.allocate( video.getWidth(), video.getHeight());

	girassol.load("../data/girassol.png");

	// 0 output channels, 
	// 2 input channels
	// 44100 samples per second
	// BUFFER_SIZE samples per buffer
	// 4 num buffers (latency)
	
	ofSoundStreamSetup(0,2,this, 44100,BUFFER_SIZE, 4);	
	left = new float[BUFFER_SIZE];
	right = new float[BUFFER_SIZE];

	for (int i = 0; i < NUM_WINDOWS; i++)
	{
		for (int j = 0; j < BUFFER_SIZE/2; j++)
		{
			freq[i][j] = 0;	
		}
	}

	//Set spectrum values to 0
	for (int i=0; i<N; i++) {
		spectrum[i] = 0.0f;
	}
}

//--------------------------------------------------------------
void ofApp::update() {

	//Compute dt
	float time = ofGetElapsedTimef();
	float dt = ofClamp( time - time0, 0, 0.1 );
	time0 = time;

	ofBackground(0, 0, 0);

	// Aloca memoria com tamanho da viewport
	vh = ofGetHeight();
	vw = ofGetWidth();

	//Atualiza som recebido e calcula espectro
	static int index=0;
	float avg_power = 0.0f;	
	
	if(index < 80)
		index += 1;
	else
		index = 0;
	
	myfft.powerSpectrum(0,(int)BUFFER_SIZE/2, left,BUFFER_SIZE,&magnitude[0],&phase[0],&power[0],&avg_power);
	
	for(int j=1; j < BUFFER_SIZE/2; j++) {
		magniView[j] = ofLerp(magniView[j], magnitude[j], 0.08);		
	}

	for(int j=1; j < BUFFER_SIZE/2; j++) {
		freq[index][j] = magnitude[j];		
	}

	for ( int i=0; i<N; i++ ) {
		spectrum[i] *= 0.95; //0.97;	//Slow decreasing
		spectrum[i] = max( spectrum[i], magniView[i] );
	}

	// Calcula beat
	beat = 0;
	int beatSize = 2;
	for ( int i=0; i<beatSize; i++ ) {
		beat += magniView[i];
	}
	beat /= beatSize;

	//Delete inactive beats
	int i=0;
	while (i < b.size()) {
		if ( !b[i].live ) {
			b.erase( b.begin() + i );
		}
		else {
			i++;
		}
	}

	//Born new beats
	beatCount += dt;      //Soma tempo passado pra limitar criação de beats
	float beatPerSec = 2.;
	if ( beatCount >= 1/beatPerSec  && beat > beatMin ) {          //It's time to born beats
		cout << "\nBeat:" << beat;
		int beatN = int( beatCount*beatPerSec );//How many born
		beatCount -= beatN/beatPerSec;          //Correct beatCount value
		for (int i=0; i<beatN; i++) {
			Beat newB;
			newB.setup();            //Start a new beat
			b.push_back( newB );     //Add this beat to array
		}
	}

	//Update beats
	for (int i=0; i<b.size(); i++) {
		b[i].update( dt );
	}


	kinect.update();
	
	// Só executa se aconteceu algo no kinect
	if(kinect.isFrameNew()) {
		
		// load grayscale depth image from the kinect source
		grayImage.setFromPixels(kinect.getDepthPixels());
		getNearMirror(grayImage, 188);

		// load grayscale depth image from the kinect source
		blurImage = grayImage;
		getBlurImage(blurImage, 121);

		// Calcula "baricentro"

		unsigned char * pix = blurImage.getPixels();
		int blurWidth = blurImage.width;
		ofPoint centro( blurWidth/2, blurImage.height/2 );
		int whiteTotal = 0;

		ofPoint baricentro0 = baricentro;

		baricentro.set(0, 0);
		for(int j = 0; j < 10; j++) {
			ofVec2f vecDirecao(15 + j*30,0);
			for(int i = 0; i < 8; i++) {
				ofPoint pontoRef = centro+vecDirecao;
				// Caso o ponto esteja dentro da imagem
				if (pontoRef.x >= 0 && pontoRef.y >= 0 && 
					pontoRef.x < blurWidth && pontoRef.y < blurImage.height) 
				{

					int white = pix[int(pontoRef.y)*blurWidth + int(pontoRef.x)];
					if (DEBUGMODE) {
						pix[int(pontoRef.y)*blurWidth + int(pontoRef.x)] = 255;
					}
					whiteTotal += white;
					baricentro += pontoRef*white;
				}

				vecDirecao.rotate(45);
			}
		}

		if(whiteTotal>0) {
			baricentro /= whiteTotal;
		} else {
			baricentro.set( blurImage.width/2, blurImage.height/2 );
		}

		baricentro = baricentro*0.04 + baricentro0*0.96;

		// Encontra contornos
		contourFinder.findContours(grayImage, 10, (kinect.width*kinect.height)/2, 20, false);
	}


	//Atualiza frame do video
	video.update();
	

}

void ofApp::getBlurImage(ofxCvGrayscaleImage &imgBlur, int indiceBlur) {
	imgBlur.blur(11);
	imgBlur.erode();
	imgBlur.dilate();
	imgBlur.blur(101);
	imgBlur.erode();
	imgBlur.dilate();
	imgBlur.blur(101);

	imgBlur.flagImageChanged();
}	


void ofApp::getNearMirror(ofxCvGrayscaleImage &imgGray, int contrasteDistancia) {

	imgGray.mirror(false,true);
	ofPixels & pixNoise = imgGray.getPixels();
	int numPixelsNoise = pixNoise.size();
	for (int i = 0; i < numPixelsNoise; i++) {
		pixNoise[i] = ofClamp(ofMap(pixNoise[i], 0, 255, -contrasteDistancia, 255), 0, 255); // Aumenta contraste de distancia
	}
	imgGray.flagImageChanged();
}

//--------------------------------------------------------------
void ofApp::draw() {
	
	ofSetColor(255, 255, 255);
	

	if(DEBUGMODE) {
		if(bDrawPointCloud) {
			easyCam.begin();
			drawPointCloud();
			easyCam.end();
		} else {
			// draw from the live kinect
			
			kinect.drawDepth(10, 10, 200, 150);
			kinect.draw(210, 10, 200, 150);
			
			grayImage.draw(10, 160, 200, 150);
			
		}
	}
    

	// acelerometro e fps
	if(DEBUGMODE) {

		ofSetColor(255, 255, 255);
		stringstream reportStream;
	        
	    if(kinect.hasAccelControl()) {
	        reportStream << "acc: " << ofToString(kinect.getMksAccel().x, 2) << " / "
	        << ofToString(kinect.getMksAccel().y, 2) << " / "
	        << ofToString(kinect.getMksAccel().z, 2) << endl;
	    }
		reportStream << "fps: " << ofGetFrameRate() << endl; 
		ofDrawBitmapString(reportStream.str(), 20, 652);
	}

	// depthcam+rgb pelo shader
	if(false) {
		fbo.begin();
		
		ofSetColor( 255, 255, 255 );
		grayImage.draw(0, 0, kinect.width, kinect.height);

		fbo.end();


		//Enable shader
		shader.begin();

		shader.setUniformTexture( "texture1", fbo.getTextureReference(), 1 ); //"1" means that it is texture 1

		ofSetColor( 255, 255, 255 );
		kinect.draw( 210, 160, 200, 150);

		shader.end();
	}

	// Video invertido pela depthcam
	if(false) {
		// Desenha depthcam no fbo pra usar de textura
		fboVideo.begin();
		
		ofSetColor( 255, 255, 255 );
		grayImage.draw(0, 0, video.getWidth(), video.getHeight());

		fboVideo.end();

		// desenha o video invertido
		shaderInvert.begin();

		video.draw( 0, 0, 1024, 768);

		shaderInvert.end();

		//desenha video filtrado pela depthcam
		shader.begin();
				
		shader.setUniformTexture( "texture1", fboVideo.getTextureReference(), 1 ); //"1" means that it is texture 1

		//Draw video through shader
		ofSetColor( 255, 255, 255 );
		video.draw( 0, 0, 1024, 768);
		
		shader.end();
	}

    // Desenha beats
	for (int i=0; i<b.size(); i++) {
		b[i].draw();
	}

    // Corrige a proporção do ponto para a tela	
	ofPoint lookAt;
	lookAt.x = ofMap(baricentro.x, 0, blurImage.width, 0, vw);
	lookAt.y = ofMap(baricentro.y, 0, blurImage.height, 0, vh);
   
	// Desenha objeto que "olha" pra pessoa
	glPushMatrix();

	glTranslatef(vw/2,vh/2, 0);
	float anguloX = ofMap(lookAt.x,0,vw,-50,50);
	glRotatef(anguloX, 0, 1, 0); 

	float anguloY = ofMap(lookAt.y,0,vh,-50,50);
	glRotatef(anguloY, -1, 0, 0); 

	ofSetColor(255, 255, 255);
	girassol.setAnchorPercent(0.5, 0.5);
	girassol.draw(0,0);

	glPopMatrix();


	if(DEBUGMODE) {

		// Acompanha posição da pessoa
    	// blurImage.draw(0,0, vw,vh);
		ofSetColor(255, 255, 255);
		contourFinder.draw(0,0, vw,vh);
		ofDrawCircle(lookAt, 10);

		/* draw the FFT */
		for (int i = 1; i < (int)(BUFFER_SIZE/4); i++){
			ofSetColor(250,250,250);
			ofLine(200+(i*8),700,200+(i*8),700-spectrum[i]*10.0f);
		}
	}
    
}

void ofApp::drawPointCloud() {
	int w = 640;
	int h = 480;
	ofMesh mesh;
	mesh.setMode(OF_PRIMITIVE_POINTS);
	int step = 2;
	for(int y = 0; y < h; y += step) {
		for(int x = 0; x < w; x += step) {
			if(kinect.getDistanceAt(x, y) > 0) {
				mesh.addColor(kinect.getColorAt(x,y));
				mesh.addVertex(kinect.getWorldCoordinateAt(x, y));
			}
		}
	}
	glPointSize(3);
	ofPushMatrix();
	// the projected points are 'upside down' and 'backwards' 
	ofScale(1, -1, -1);
	ofTranslate(0, 0, -1000); // center the points a bit
	ofEnableDepthTest();
	mesh.drawVertices();
	ofDisableDepthTest();
	ofPopMatrix();
}

//--------------------------------------------------------------
void ofApp::exit() {
	kinect.setCameraTiltAngle(0); // zero the tilt on exit
	kinect.close();
}

//--------------------------------------------------------------
void ofApp::audioReceived 	(float * input, int bufferSize, int nChannels){	
	// samples are "interleaved"
	for (int i = 0; i < bufferSize; i++){
		left[i] = input[i*2];
		right[i] = input[i*2+1];
	}
	bufferCounter++;
}

//--------------------------------------------------------------
void ofApp::keyPressed (int key) {
	switch (key) {
		case ' ':
			break;
			
		case'p':
			bDrawPointCloud = !bDrawPointCloud;
			break;
			
		case '>':
		case '.':
			break;
			
		case '<':
		case ',':
			break;
			
		case '+':
		case '=':
			break;
			
		case '-':
			break;
			
		case 'w':
			break;
			
		case 'o':
			kinect.setCameraTiltAngle(angle); // go back to prev tilt
			kinect.open();
			break;
			
		case 'c':
			kinect.setCameraTiltAngle(0); // zero the tilt
			kinect.close();
			break;
			
		case '1':
			kinect.setLed(ofxKinect::LED_GREEN);
			break;
			
		case '2':
			kinect.setLed(ofxKinect::LED_YELLOW);
			break;
			
		case '3':
			kinect.setLed(ofxKinect::LED_RED);
			break;
			
		case '4':
			kinect.setLed(ofxKinect::LED_BLINK_GREEN);
			break;
			
		case '5':
			kinect.setLed(ofxKinect::LED_BLINK_YELLOW_RED);
			break;
			
		case '0':
			kinect.setLed(ofxKinect::LED_OFF);
			break;
			
		case OF_KEY_UP:
			angle++;
			if(angle>30) angle=30;
			kinect.setCameraTiltAngle(angle);
			break;
			
		case OF_KEY_DOWN:
			angle--;
			if(angle<-30) angle=-30;
			kinect.setCameraTiltAngle(angle);
			break;
	}
}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button)
{
	
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button)
{

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button)
{

}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h)
{

}