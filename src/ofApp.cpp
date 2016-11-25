#include "ofApp.h"

const int N = 256;		//Number of bands in spectrum
float spectrum[ N ];	//Smoothed spectrum values
float beat;
float beatMin = 8;
int vw,vh;
float beatCount = 0;
float time0;
ofPoint baricenter(0,0), baricentro(0,0);

vector<Beat> b;	  //beats

vector<PolyMask> masks;	//Array of sliders
ofPoint lookAt;
ofImage olho, orbita, girassol;
ofxCvGrayscaleImage grayImage, blurImage; // grayscale depth image
ofxKinect kinect;

ofxCvColorImage colorImg;

ofxCvContourFinder contourFinder;
ofVideoPlayer video, sereiasloucas;

ofShader shader, shaderInvert; //Shader

ofFbo fbo, fboVideo; //Buffers temporarios

ofColor corMatiz, corMatizComplementar;

float inicioFbo[4];
int numeroBrisaFbo[4];
float tempoBrisa = 20, tempoFade = 1;
float ultimoEvento;

int whiteTotalSlow = 0; // quantidade de coisa na frente da tela

//variaveis pra usar de vez em quando
int intControl,floatControl;

bool DEBUGMODE = false;
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
void Beat::draw(int vertices){
	if ( live ) {
		//Compute color
		ofColor color = corMatiz;
		ofSetColor( color );


		desenhaPoligono(vertices,size*vh, false, false);
	}
}

//--------------------------------------------------------------
void ofApp::setup() {
	// inicia cores
	corMatiz = ofColor::red;

	intControl = 0;
	// Aloca memoria com tamanho da viewport
	vh = 768;
	vw = 1024;

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
	
	//aloca "layers"

	fboLayer[0].allocate(vw, vh);
	fboLayer[1].allocate(vw, vh);
	fboLayer[2].allocate(vw, vh);
	fboLayer[3].allocate(vw, vh);

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

	sereiasloucas.load("sereiasloucas.mp4");
	sereiasloucas.play();


	fboVideo.allocate( video.getWidth(), video.getHeight());
	fbo.allocate(kinect.width, kinect.height);

	girassol.load("../data/girassol.png");
	olho.load("../data/olhoillu.png");
	orbita.load("../data/orbitaillu.png");

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

	// Controle de fbo
	for(int i = 0; i < 4; i++) {
		inicioFbo[i] = -99;
		numeroBrisaFbo[i] = 4-i;
	}
	ultimoEvento = -99;

}

//--------------------------------------------------------------
void ofApp::update() {
	corMatiz.setHue( int(time0*5)%360 );
	corMatizComplementar.set(corMatiz);
	corMatizComplementar.setHue( (int(time0*5)+200) %360);
	//Compute dt
	float time = ofGetElapsedTimef();
	float dt = ofClamp( time - time0, 0, 0.1 );
	time0 = time;

	ofBackground(0, 0, 0);

	if(DEBUGMODE) {
		// Aloca memoria com tamanho da viewport
		vh = ofGetHeight();
		vw = ofGetWidth();
	}

	//Atualiza som recebido e calcula espectro
	static int index=0;
	float avg_power = 0.0f;	
	
	if(index < 80)
		index += 1;
	else
		index = 0;
	
	//myfft.powerSpectrum(0,(int)BUFFER_SIZE/2, left,BUFFER_SIZE,&magnitude[0],&phase[0],&power[0],&avg_power);
	
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
		int whiteTotal = 0; // quantidade de coisa na frente da tela
		ofPoint centro( blurWidth/2, blurImage.height/2 );

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


		// Define pra onde vai ser o foco de visão (baricentro da profundidade)
	    // Corrige a proporção do ponto para a tela	
		
		lookAt.x = ofMap(baricentro.x, 0, blurImage.width, 0, vw);
		lookAt.y = ofMap(baricentro.y, 0, blurImage.height, 0, vh);
		
		whiteTotalSlow = whiteTotalSlow*0.9 + whiteTotal*0.1;

	}

	ofPoint perlinPoint;
	perlinPoint.x = vw * ofNoise( time0 * 0.15 );
	perlinPoint.y = vh * ofNoise( time0 * 0.2 );	

	if( whiteTotalSlow < 1000 || whiteTotalSlow > 10000) {
		lookAt = perlinPoint;
	}
		cout << "\nwhiteTotalSlow:" << whiteTotalSlow;
	
	//Atualiza frame do video
	video.update();
	sereiasloucas.update();
	
	//Ativa evento
	if(time0 - ultimoEvento > tempoBrisa) {
		ultimoEvento = time0;

		float eventoRand = ofRandom(0,1);
		cout << "\neventoRand:" << eventoRand;
		if(eventoRand < 0.2) {
			//Girassol com beats
			inicioFbo[0] = time0;
			inicioFbo[1] = time0;
			inicioFbo[2] = time0;
			inicioFbo[3] = time0;
			numeroBrisaFbo[0] = 0; // null
			numeroBrisaFbo[1] = 3; // Losangos com beat
			numeroBrisaFbo[2] = 1; // Girassolho
			numeroBrisaFbo[3] = 0; // null
		} else if(eventoRand < 0.4){
			//illuminati
			inicioFbo[0] = time0;
			inicioFbo[1] = time0;
			inicioFbo[2] = time0;
			numeroBrisaFbo[0] = 0; // null
			numeroBrisaFbo[1] = 7; // triangulos
			numeroBrisaFbo[2] = 2; // olho illu
		} else if(eventoRand < 0.45){

			//video
			inicioFbo[0] = time0;
			inicioFbo[1] = time0;
			inicioFbo[2] = time0;
			numeroBrisaFbo[0] = 0; // null
			numeroBrisaFbo[1] = 0; // null
			numeroBrisaFbo[2] = 5; // video floresta

		} else if(eventoRand < 0.7){
			//video
			inicioFbo[0] = time0;
			inicioFbo[1] = time0;
			inicioFbo[2] = time0;
			numeroBrisaFbo[0] = 0; // null
			numeroBrisaFbo[1] = 0; // null
			numeroBrisaFbo[2] = 9; // video sereias
		}
		

	}

	// Atualiza brisa se ja passou do tempo
	if(time0 - inicioFbo[0] > tempoBrisa) {
		float eventoRand = ofRandom(0,1);
		if(eventoRand < 0.85) {
			numeroBrisaFbo[0] = 4; // varios poligonos
		} else if(eventoRand < 0.9) {
			numeroBrisaFbo[0] = 3; // beats quadrados
		} else {
			numeroBrisaFbo[0] = 7; // beats quadrados
		}
		inicioFbo[0] = time0;
	}
	if(time0 - inicioFbo[1] > tempoBrisa) {
		numeroBrisaFbo[1] = 0; // null
		inicioFbo[1] = time0;
	}
	if(time0 - inicioFbo[2] > tempoBrisa) {
		float eventoRand = ofRandom(0,1);
		if(eventoRand < 0.5) {
			numeroBrisaFbo[2] = 1; // girasoll
		} else if(eventoRand < 0.9) {
			numeroBrisaFbo[2] = 2; // illuminati
		} else {
			numeroBrisaFbo[2] = 5; // video
		}
		inicioFbo[2] = time0;
	}
	if(time0 - inicioFbo[3] > tempoBrisa) {
		float eventoRand = ofRandom(0,1);
		if(eventoRand < 0.6) {
			numeroBrisaFbo[3] = 6; // contornos
		} else {
			numeroBrisaFbo[3] = 0; // null
		}
		inicioFbo[3] = time0;
	}

	fboLayer[0].begin();
		ofBackground(0, 0, 0, 0);		
		desenhaBrisa(numeroBrisaFbo[0]);
	fboLayer[0].end();

	fboLayer[1].begin();
		ofBackground(0, 0, 0, 0);
		desenhaBrisa(numeroBrisaFbo[1]);
	fboLayer[1].end();

	fboLayer[2].begin();
		ofBackground(0, 0, 0, 0);
		desenhaBrisa(numeroBrisaFbo[2]);
	fboLayer[2].end();

	fboLayer[3].begin();
		ofBackground(0, 0, 0, 0);
		desenhaBrisa(numeroBrisaFbo[3]);
	fboLayer[3].end();

}

void desenhaBrisa(int nBrisa) {
	switch (nBrisa) {
		case 0:
			//desenha Nada
		break;

		case 1:
			desenhaOlhoGirassol();
		break;

		case 2:
			desenhaOlhoIllu();
		break;

		case 3:
			desenhaBeats(4);
		break;

		case 4:
			desenhaVariosPoligonos();
		break;

		case 5:
			desenhaCamFloresta();
		break;

		case 6:
			desenhaContorno();
		break;

		case 7:
			desenhaBeats(3);
		break;

		case 8:
			desenhaDepthAlpha();
		break;

		case 9:
			desenhaCamSereias();
		break;
	}
}

void desenhaVariosPoligonos() {
	// Desenha poligono
	ofSetColor(corMatizComplementar);
	desenhaPoligono( int(abs(sin(time0*0.1)*4)) + 4, abs(sin(time0*0.7))*100 + 600 ,true,true);

	// Desenha poligono
	ofSetColor(corMatiz);
	desenhaPoligono( int(abs(sin(time0*0.1)*4)) + 4, abs(sin(time0*0.7))*100 + 500 ,true,true);

	// Desenha poligono
	ofSetColor(corMatizComplementar);
	desenhaPoligono( int(abs(sin(time0*0.1)*4)) + 4, abs(sin(time0*0.7+.1))*100 + 400 ,true,true);

	// Desenha poligono
	ofSetColor(corMatiz);
	desenhaPoligono( int(abs(sin(time0*0.1)*4)) + 4, abs(sin(time0*0.7+.2))*100 + 300 ,true,true);

	// Desenha poligono
	ofSetColor(corMatizComplementar);
	desenhaPoligono( int(abs(sin(time0*0.1)*4)) + 4, abs(sin(time0*0.7+.3))*100 + 200 ,true,true);

	// Desenha poligono
	ofSetColor(corMatiz);
	desenhaPoligono( int(abs(sin(time0*0.1)*4)) + 4, abs(sin(time0*0.7)+.4)*100 + 100 ,true,true);
}

//--------------------------------------------------------------
void ofApp::draw() {
	
	ofBackground(0, 0, 0);
	


	// Desenha Brisas
	for(int i = 0; i < 4; i++) {

		// Testa se ja passou tempo pra começar o fade && ainda nao acabou a brisa
		if(time0 - inicioFbo[i] > (tempoBrisa - tempoFade) && time0 - inicioFbo[i] < tempoBrisa) {
			ofSetColor(255, 255, 255, ofMap(time0,inicioFbo[i] + tempoBrisa - tempoFade,inicioFbo[i] + tempoBrisa,255,0));
		}
		//Testa se ta no comecinho da brisa
		else if(time0 - inicioFbo[i] < tempoFade) {
			ofSetColor(255, 255, 255, ofMap(time0,inicioFbo[i],inicioFbo[i]+tempoFade,0,255));
		} else {
			ofSetColor(255, 255, 255);
		}
		fboLayer[i].draw(0,0);
	}
    

	if(DEBUGMODE) {
		// cameras e visualizações
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

		// acelerometro e fps
		ofSetColor(255, 255, 255);
		stringstream reportStream;
	        
	    if(kinect.hasAccelControl()) {
	        reportStream << "acc: " << ofToString(kinect.getMksAccel().x, 2) << " / "
	        << ofToString(kinect.getMksAccel().y, 2) << " / "
	        << ofToString(kinect.getMksAccel().z, 2) << endl;
	    }
		reportStream << "fps: " << ofGetFrameRate() << endl; 
		ofDrawBitmapString(reportStream.str(), 20, 652);

		// Acompanha posição da pessoa
    	blurImage.draw(0,0, vw,vh);
		ofSetColor(255, 255, 255);
		contourFinder.draw(0,0, vw,vh);
		ofDrawCircle(lookAt, 10);

		/* draw the FFT */
		for (int i = 1; i < (int)(BUFFER_SIZE/4); i++){
			ofSetColor(250,250,250);
			ofLine(200+(i*8),700,200+(i*8),700-spectrum[i]*10.0f);
		}
	}

	for (int i = 0; i<masks.size(); i++) {
		PolyMask &m = masks[i];
		m.draw();
	}

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

//	imgGray.mirror(false,true);
	ofPixels & pixNoise = imgGray.getPixels();
	int numPixelsNoise = pixNoise.size();
	for (int i = 0; i < numPixelsNoise; i++) {
		pixNoise[i] = ofClamp(ofMap(pixNoise[i], 0, 255, -contrasteDistancia, 255), 0, 255); // Aumenta contraste de distancia
	}
	imgGray.flagImageChanged();
}

// Desenha poligono
void desenhaBeats(int vertices) {

	ofSetColor(corMatiz);

	desenhaPoligono(vertices,vh/2, false, false);
	// Desenha beats
	for (int i=0; i<b.size(); i++) {
		b[i].draw(vertices);
	}
}


// Desenha girassol que "olha" pra pessoa
void desenhaOlhoGirassol() {
	glPushMatrix();

	glTranslatef(vw/2,vh/2, 0);

	float anguloX = ofMap(lookAt.x,0,vw,-50,50);
	glRotatef(anguloX, 0, 1, 0); 

	float anguloY = ofMap(lookAt.y,0,vh,-50,50);
	glRotatef(anguloY, -1, 0, 0); 
	glRotatef(anguloY+anguloX, 0, 0, 0.2); 

	ofSetColor(255, 255, 255);
	girassol.setAnchorPercent(0.5, 0.5);
	girassol.draw(0,0);

	glPopMatrix();
}

// Desenha piramedo zoiuda que "olha" pra pessoa
void desenhaOlhoIllu() {
	glPushMatrix();

	glTranslatef(vw/2,vh/2, 0);

	ofPoint moveOlho;
	moveOlho.x = ofMap(lookAt.x,0,vw,-25,25);
	moveOlho.y = ofMap(lookAt.y,0,vh,-15,15);

	olho.setAnchorPercent(0.5, 0.5);
	orbita.setAnchorPercent(0.5, 0.5);

	olho.draw(moveOlho);
	orbita.draw(0,0);

	glPopMatrix();
}

// Desenha depthcam+rgb pelo shader
void desenhaDepthAlpha() {
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

// Desenha Video invertido pela depthcam
void desenhaCamFloresta() {

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

	//desenha video pelo shader
	ofSetColor( 255, 255, 255 );
	video.draw( 0, 0, 1024, 768);
	
	shader.end();
}

// Desenha Video invertido pela depthcam
void desenhaCamSereias() {

	// Desenha depthcam no fbo pra usar de textura
	fboVideo.begin();
	
	ofSetColor( 255, 255, 255 );
	grayImage.draw(0, 0, video.getWidth(), video.getHeight());

	fboVideo.end();

	// desenha o video invertido
	shaderInvert.begin();

	sereiasloucas.draw( 0, 0, 1024, 768);

	shaderInvert.end();

	//desenha video filtrado pela depthcam
	shader.begin();
			
	shader.setUniformTexture( "texture1", fboVideo.getTextureReference(), 1 ); //"1" means that it is texture 1

	//desenha video pelo shader
	ofSetColor( 255, 255, 255 );
	sereiasloucas.draw( 0, 0, 1024, 768);
	
	shader.end();
}

// Desenha poligono
void desenhaPoligono(int vertices, int radius, bool rotate, bool fill) {
	if(vertices < 3)
		vertices = 3;
	//corrigindo tamanho aparente
	radius = radius - vertices*8;
	if(fill) {
		ofFill();
	}else {
		ofNoFill();
	}
	glPushMatrix();

	//Gambizinha pra caso seja o triangulo com beat
	if(vertices == 3 && !rotate){
		glTranslatef(vw/2,vh/2+70, 0);
	} else {
		// o que aocntecia normalmente antes da gambi
		glTranslatef(vw/2,vh/2, 0);
	}

	if (rotate) {
		glRotatef( sin(time0*0.5)*50 + 	(360/vertices-1)-(360/vertices) + 100, 0, 0, 1); 
	} else {
		glRotatef( 180, 0, 0, 1); 
	}

	ofBeginShape();
	for(int i = 0; i < vertices+1; i++) {
		float theta = (TWO_PI/vertices) * i;
		ofVertex( sin(theta)*radius , cos(theta)*radius ,0);
	}
	ofEndShape();

	glPopMatrix();
}

void desenhaContorno() {
	ofSetColor(255, 255, 255);
	contourFinder.draw(0,0, vw,vh);
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
			
		case 'd':
			DEBUGMODE = !DEBUGMODE;
			break;
			
		case '>':
		case '.':
			break;
			
		case '<':
		case ',':
			break;
			
		case '+':
		case '=':
			intControl++;
			cout << "\n intControl:" << intControl;
			break;
			
		case '-':
		case '_':
			intControl--;
			cout << "\n intControl:" << intControl;
			break;
			
		case 'w':
			break;

		case 'm':
			addMask();
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
	
	for (int i = 0; i<masks.size(); i++) {
		PolyMask &m = masks[i];
		m.mouseDragged(x,y);
	}
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button)
{

	for (int i = 0; i<masks.size(); i++) {
		PolyMask &m = masks[i];
		m.mousePressed(x,y);
	}
}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button)
{
	for (int i = 0; i<masks.size(); i++) {
		PolyMask &m = masks[i];
		m.mouseReleased(x,y);
	}
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


void addMask() {
	PolyMask mask;
	mask.setup();
	masks.push_back(mask);
}
//--------------------------------------------------------------
//----------------------  GUI ----------------------------------
//--------------------------------------------------------------
void PolyMask::setup() {
	ponto1.set(vw/2,vh/3);
	ponto2.set(vw/4,vh*0.8);
	ponto3.set(vw*0.75,vh*0.8);
	pontoDragged.set(0,0);
	p1a = false;
	p2a = false;
	p3a = false;
}

void PolyMask::draw() {
	ofFill();
	ofSetColor(0,0,0);

	ofBeginShape();
		ofVertex( ponto1.x, ponto1.y ,0);
		ofVertex( ponto2.x, ponto2.y ,0);
		ofVertex( ponto3.x, ponto3.y ,0);
	ofEndShape();

	ofSetColor(255,255,255);
	if(p1a) {
		ofDrawCircle(ponto1, 10);
	}
	if(p2a) {
		ofDrawCircle(ponto2, 10);
	}
	if(p3a) {
		ofDrawCircle(ponto3, 10);
	}
}

void PolyMask::mousePressed(int x, int y) {
	if(pontoClicked.x == 0 && pontoClicked.y == 0) {
		pontoClicked.set(x,y);
	}
	
	if(ponto1.distance(pontoClicked) < 10) {
		p1a = true;
	} else if(ponto2.distance(pontoClicked) < 10) {
		p2a = true;
	} else if(ponto3.distance(pontoClicked) < 10) {
		p3a = true;
	} else if( PointInTriangle(pontoClicked) ) {
		p1a = true;
		p2a = true;
		p3a = true;
	}

}

void PolyMask::mouseDragged(int x, int y) {
	ofPoint p(x,y);
	pontoDragged = p - pontoClicked;
}

void PolyMask::mouseReleased(int x, int y) {
	ofPoint p(x,y);
	if(p1a) {
		ponto1 += p - pontoClicked;
	}
	if(p2a) {
		ponto2 += p - pontoClicked;
	}
	if(p3a) {
		ponto3 += p - pontoClicked;
	}
	pontoDragged.set(0,0);
	p1a = false;
	p2a = false;
	p3a = false;
}

float PolyMask::sign (ofPoint p1, ofPoint p2, ofPoint p3)
{
    return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
}

bool PolyMask::PointInTriangle (ofPoint pt)
{
    bool b1, b2, b3;

    b1 = sign(pt, ponto1, ponto2) < 0.0f;
    b2 = sign(pt, ponto2, ponto3) < 0.0f;
    b3 = sign(pt, ponto3, ponto1) < 0.0f;

    return ((b1 == b2) && (b2 == b3));
}
