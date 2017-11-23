#include <iostream>
#include <stdio.h>
#include "opencv2/opencv.hpp"
#include <raspicam/raspicam_cv.h>
#include<stdio.h>
#include<string.h>    //strlen
#include<stdlib.h>    //strlen
#include<sys/socket.h>
#include<arpa/inet.h> //inet_addr
#include<unistd.h>    //write
#include<pthread.h> //for threading , link with lpthread
#include <semaphore.h> 
 
//the thread function
void *connection_handler(void *);
void *mainTCP(void *pVoid);
 

using namespace std;
using namespace cv;

/** Function Headers */
void detectAndDisplayBlob( Mat frame ,Ptr<SimpleBlobDetector> detector);

/** Global variables */
String window_name = "Capture - Face detection";
sem_t kpready;
vector<KeyPoint> keypoints; // Storage for blobs

/** @function main */
int main( int argc, const char** argv )
{
	raspicam::RaspiCam_Cv Camera;
    Mat frame;  
	Ptr<SimpleBlobDetector> detector;
		
	Camera.set( CV_CAP_PROP_FORMAT, CV_8UC3 );
	Camera.set( CV_CAP_PROP_FRAME_WIDTH  , 640 );
	Camera.set( CV_CAP_PROP_FRAME_HEIGHT  , 480 );
 	Camera.set( CV_CAP_PROP_EXPOSURE  , -1 );
 	if (!Camera.open()) {cerr<<"Error opening the camera"<<endl;return -1;}
    
	// Setup SimpleBlobDetector parameters.
	SimpleBlobDetector::Params params;

	// Change thresholds
	params.minThreshold = 10;
	params.maxThreshold = 200;
	params.thresholdStep = 20;
	
	// Filter by Area.
	params.filterByArea = true;
	params.minArea = 100;
	params.maxArea = 100000;

	// Filter by Circularity
	params.filterByCircularity = true;
	params.minCircularity = 0.8;

	// Filter by Convexity
	params.filterByConvexity = true;
	params.minConvexity = 0.8;

	// Filter by Inertia
	params.filterByInertia = true;
	params.minInertiaRatio = 0.8;



	// Set up detector with params
	 detector = SimpleBlobDetector::create(params);   

	 
	sem_init(&kpready, 0, 1);
	pthread_t tcp_thread;
        
	 
	if( pthread_create( &tcp_thread , NULL ,  mainTCP , NULL) < 0)
	{
		perror("could not create tcp_thread");
		return 1;
	}
	
    while ( 1 )
    {
        Camera.grab();
        Camera.retrieve ( frame);

        if( frame.empty() )
        {
            printf(" --(!) No captured frame -- Break!");
            break;
        }

        //-- 3. Apply the classifier to the frame
        detectAndDisplayBlob( frame ,detector);
		
		
        char c = (char)waitKey(10);
       if( c == 27 ) { break; } // escape
    }
    return 0;
}

void detectAndDisplayBlob( Mat frame,Ptr<SimpleBlobDetector> detector )
{
	
	detector->detect( frame, keypoints);

		// Draw detected blobs as red circles.
	// DrawMatchesFlags::DRAW_RICH_KEYPOINTS flag ensures
	// the size of the circle corresponds to the size of blob
	if(keypoints.size()>0)		
		sem_post(&kpready);
	
	
	for(int ii=0;ii<keypoints.size();ii++)
	{
		printf("kp %i %f %f\n",ii,keypoints[ii].pt.x,keypoints[ii].pt.y);
	}

	// Draw detected blobs as red circles.
	// DrawMatchesFlags::DRAW_RICH_KEYPOINTS flag ensures
	// the size of the circle corresponds to the size of blob

	Mat im_with_keypoints;
	drawKeypoints( frame, keypoints, im_with_keypoints, Scalar(0,0,255), DrawMatchesFlags::DRAW_RICH_KEYPOINTS );

	// Show blobs
	imshow("keypoints", im_with_keypoints );
	 
}



void *mainTCP(void *pVoid)
{
    int socket_desc , client_sock , c , *new_sock;
    struct sockaddr_in server , client;
     
    //Create socket
    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_desc == -1)
    {
        printf("Could not create socket");
    }
    puts("Socket created");
     
    //Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons( 8888 );
     
    //Bind
    if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
    {
        //print the error message
        perror("bind failed. Error");
        return NULL;
    }
    puts("bind done");
     
    //Listen
    listen(socket_desc , 3);
     
    //Accept and incoming connection
    puts("Waiting for incoming connections...");
    c = sizeof(struct sockaddr_in);
     
     
    //Accept and incoming connection
    puts("Waiting for incoming connections...");
    c = sizeof(struct sockaddr_in);
    while( (client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)) )
    {
        puts("Connection accepted");
         
        pthread_t sniffer_thread;
        new_sock =  (int*)malloc(1);
        *new_sock = client_sock;
         
        if( pthread_create( &sniffer_thread , NULL ,  connection_handler , (void*) new_sock) < 0)
        {
            perror("could not create thread");
            return NULL;
        }
         
        //Now join the thread , so that we dont terminate before the thread
        //pthread_join( sniffer_thread , NULL);
        puts("Handler assigned");
    }
     
    if (client_sock < 0)
    {
        perror("accept failed");
        return NULL;
    }
     
    return NULL;
}
 
/*
 * This will handle connection for each client
 * */
void *connection_handler(void *socket_desc)
{
    //Get the socket descriptor
    int sock = *(int*)socket_desc;
    int read_size;
    char *message , client_message[2000];
    char buf [128];
	
    //Send some messages to the client
    message = "Greetings! I am your connection handler \r\n";
    write(sock , message , strlen(message));
     
    message = "Now type something and i shall repeat what you type \r\n";
    write(sock , message , strlen(message));
     
    //Receive a message from client
    while(1)
    {
		sem_wait(&kpready); 
        //Send the message back to client
		for(int ii=0;ii<keypoints.size();ii++)
		{
			sprintf(buf,"kp %i %f %f\n\r",ii,keypoints[ii].pt.x,keypoints[ii].pt.y);
			write(sock , buf , strlen(buf));
		}
		
       
    }
     
    if(read_size == 0)
    {
        puts("Client disconnected");
        fflush(stdout);
    }
    else if(read_size == -1)
    {
        perror("recv failed");
    }
         
    //Free the socket pointer
    free(socket_desc);
     
    return 0;
}
/** @function detectAndDisplay */
/*
void detectAndDisplay( Mat frame )
{
    std::vector<Rect> faces;
    Mat frame_gray;

    cvtColor( frame, frame_gray, COLOR_BGR2GRAY );
    equalizeHist( frame_gray, frame_gray );

    //-- Detect faces
    face_cascade.detectMultiScale( frame_gray, faces, 1.1, 2, 0|CASCADE_SCALE_IMAGE, Size(30, 30) );

    for ( size_t i = 0; i < faces.size(); i++ )
    {
        Point center( faces[i].x + faces[i].width/2, faces[i].y + faces[i].height/2 );
        ellipse( frame, center, Size( faces[i].width/2, faces[i].height/2 ), 0, 0, 360, Scalar( 255, 0, 255 ), 4, 8, 0 );

        Mat faceROI = frame_gray( faces[i] );
        std::vector<Rect> eyes;

        //-- In each face, detect eyes
        eyes_cascade.detectMultiScale( faceROI, eyes, 1.1, 2, 0 |CASCADE_SCALE_IMAGE, Size(30, 30) );

        for ( size_t j = 0; j < eyes.size(); j++ )
        {
            Point eye_center( faces[i].x + eyes[j].x + eyes[j].width/2, faces[i].y + eyes[j].y + eyes[j].height/2 );
            int radius = cvRound( (eyes[j].width + eyes[j].height)*0.25 );
            circle( frame, eye_center, radius, Scalar( 255, 0, 0 ), 4, 8, 0 );
        }
    }
    //-- Show what you got
    imshow( window_name, frame );
}
*/
