#include "ofApp.h"

#include "ofxAdvancedXmlSettings.h"
#include "ofBaku.h"

//--------------------------------------------------------------
void ofApp::setup(){
	
	ofEnableSmoothing();
	ofBackground(20, 20, 20);
	
	oscVt.setup(8000);
	oscDf.setup(1234);
	
	grabCam.setFixUpDirectionEnabled(true);
	grabCam.setPosition(.5, .5, 1);
	grabCam.lookAt(ofVec3f());

	
	calibOrigin.set(0, 0, 0);
	calibAxisX.set(1, 0, 0);
	calibAlt.set(0, 0, 1);
	
	// setup gui
	gui.setup();
	
	// load settings
	ofxAdvancedXmlSettings settings;
	settings.load("settings.xml");
	
	enableAutoCam    = settings.getValue("enableAutoCam", false);
    autoCamDistance  = settings.getValue("autoCamDistance", 1.0);
    numSampleFrames  = settings.getValue("numSampleFrames", 10);
	showRawPose      = settings.getValue("showRawPose", false);
	currentSceneName = settings.getValue("currentSceneName", "");
	currentFrame     = settings.getValue("currentFrame", 0);
	
	calibOrigin = settings.getValue("calibOrigin",	ofVec3f(0, 0, 0));
	calibAxisX	= settings.getValue("calibAxisX",	ofVec3f(1, 0, 0));
	calibAlt	= settings.getValue("calibAlt",		ofVec3f(0, 0, 1));
	
	vtCalib		= settings.getValue("vtCalib", ofMatrix4x4());
	dirCalib	= settings.getValue("dirCalib", ofMatrix4x4());
	
	loadCurrentScene();
}

//--------------------------------------------------------------
void ofApp::exit() {
	
	ofxAdvancedXmlSettings settings;
	
	settings.setValue("enableAutoCam", enableAutoCam);
    settings.setValue("autoCamDistance", autoCamDistance);
    settings.setValue("numSampleFrames", numSampleFrames);
	settings.setValue("showRawPose", showRawPose);
	settings.setValue("currentSceneName", currentSceneName);
	settings.setValue("currentFrame", currentFrame);
	
	settings.setValue("calibOrigin", calibOrigin);
	settings.setValue("calibAxisX", calibAxisX);
	settings.setValue("calibAlt", calibAlt);
	
	settings.setValue("vtCalib", vtCalib);
	settings.setValue("dirCalib", dirCalib);
	
	settings.save("settings.xml");
	
	saveCurrentScene();
}

//--------------------------------------------------------------
void ofApp::update(){
    
    while (sheet.size() < currentFrame) {
        sheet.push_back(new Frame());
    }
	
	while (oscVt.hasWaitingMessages()) {
		
		ofxOscMessage m;
		oscVt.getNextMessage(m);
		
		static string address;
		address = m.getAddress();
		
		if (address == "/vivetracker/position") {
			
			static float x, y, z;
			
			x = m.getArgAsFloat(0);
			y = m.getArgAsFloat(1);
			z = m.getArgAsFloat(2);
			
			vtRawPose.setTranslation(x, y, z);
            
		} else if (address == "/vivetracker/quaternion") {
			
			static ofQuaternion quat;
			static float w, x, y, z;
			
			w = m.getArgAsFloat(0);
			x = m.getArgAsFloat(1);
			y = m.getArgAsFloat(2);
			z = m.getArgAsFloat(3);
			
			quat.set(x, y, z, w);
			vtRawPose.setRotate(quat);
		
		} else if (address == "/vivetracker/visible") {
			
			trackerVisible = m.getArgAsBool(0);
		}
	}
	
	vtPose = dirCalib * (vtRawPose * vtCalib);
    
    sheet[currentFrame - 1]->pos = vtPose.getTranslation();
    sheet[currentFrame - 1]->rot = vtPose.getRotate().getEuler();
	
	while (oscDf.hasWaitingMessages()) {
		
		ofxOscMessage m;
		oscDf.getNextMessage(m);
		
		static string address;
		static int frame;
		static string sceneName;
		
		address = m.getAddress();
		frame = m.getArgAsInt(0);
		sceneName = m.getArgAsString(1);
		
		if (address == "/dragonframe/position") {
			
			while (sheet.size() < frame) {
				sheet.push_back(new Frame());
			}
			
			ofLogNotice() << frame << " - resized:" << sheet.size();
			
			// TODO: Set currentFrame on startup somehow
			currentFrame = frame;
		
		} else if (address == "/dragonframe/shoot") {
			
			// TODO: Support multi projects
			currentSceneName = sceneName;
			
			while (sheet.size() < frame) {
				sheet.push_back(new Frame());
			}
			
			//ofLogNotice() << frame << " - resized:" << sheet.size();
			
			sheet[frame - 1]->pos.set(vtPose.getTranslation());
			sheet[frame - 1]->rot.set(vtPose.getRotate().getEuler());
			sheet[frame - 1]->empty = false;
			
		} else if (address == "/dragonframe/cc") {
			
			string filePath = m.getArgAsString(2);
			
			sheet[frame - 1]->hash = ofGetFileHash(filePath);
			sheetDirPath = ofFilePath::getEnclosingDirectory(filePath, false);
			
			ofLogNotice() << sheetDirPath;
			
		} else if (address == "/dragonframe/conform") {
			
			sortCurrentScene();
		}
	}
}

//--------------------------------------------------------------
void ofApp::draw() {
	
	static ofPolyline camSpline, yawGraph, pitchGraph, yGraph;
    
    // calc prev frame number
    static int prevFrame;

    for (int i = currentFrame - 2; i >= 0; i--) {
        
        if (!sheet[i]->empty) {
            prevFrame = i;
            break;
        }
    }
	
	ofPushStyle();
	ofEnableAlphaBlending();
	
	if (enableAutoCam) {
		
		// get prev position
        static ofVec3f      prevPos;
        prevPos = sheet[prevFrame - 1]->pos;
        
        static ofVec3f rp; // relative position from prevPos
        rp = grabCam.getGlobalTransformMatrix().getRotate() * ofVec3f(0, 0, autoCamDistance);
        
        grabCam.setPosition(prevPos + rp);
        grabCam.begin();
        
    }
	
	ofDisableDepthTest();
	ofPushMatrix();
	{
		ofRotateZ(90);
		ofSetColor(64);
		ofDrawGridPlane(0.1f, 30.0f, false);
		
		ofSetColor(128);
		ofDrawGridPlane(1.0f, 3.0f, true);
	}
	ofPopMatrix();
	
	ofSetLineWidth(2);
	ofDrawAxis(1.0f);
	ofEnableDepthTest();
	
	if (showRawPose) { ofPushMatrix(); ofMultMatrix(vtCalib.getInverse()); }
	{
		
		// draw camera
		ofPushMatrix(); ofMultMatrix(vtPose); ofScale(1, 1, -1);
		{
			ofSetColor(255);
			ofDrawCamera();
		}
		ofPopMatrix();
		
		// draw calib points
		if (showRawPose) {
			ofPushMatrix(); ofMultMatrix(vtCalib);
			{
				ofSetColor(255, 0, 255);
				ofDrawSphere(calibOrigin, 0.01);
				
				ofPushStyle();
				ofSetColor(255, 0, 0);
				ofSetLineWidth(5);
				ofDrawArrow(calibOrigin, calibAxisX, 0.015);
				ofPopStyle();
				
				ofSetColor(0, 255, 255);
				ofDrawSphere(calibAlt, 0.01);
			}
			ofPopMatrix();
		}
		
		// draw spline
		
		camSpline.clear();
		yawGraph.clear();
		pitchGraph.clear();
        
        // calc scale of each graphs
        static float prevYaw, prevPitch, yawScale, pitchScale;
        
        prevYaw = sheet[prevFrame - 1]->rot.y;
        prevPitch = sheet[prevFrame - 1]->rot.x;
        
        yawScale = 10.0;
        pitchScale = 10.0;
        
        static int numFrames;
        numFrames = 0;
        
        for (int i = currentFrame - 1; i >= 0; i--) {
            Frame *f = sheet[i];
            
            if (!f->empty) {
                yawScale = max(yawScale, abs(f->rot.y - prevYaw) * 2);
                pitchScale = max(pitchScale, abs(f->rot.x - prevPitch) * 2);
            }
            
            if (++numFrames == numSampleFrames) {
                break;
            }
        }
        
        yawScale += 5.0;
        pitchScale += 5.0;
        
		ofSetColor(255);
        
        float gxStep = (float)ofGetWidth() / numSampleFrames;
		
		for (int i = 0; i < sheet.size(); i++) {
			Frame *f = sheet[i];
			
			if (i + 1 != currentFrame && f->empty) {
				continue;
			}
			
			static ofVec3f pos, rot;
			
			pos = f->pos;
			rot = f->rot;
			
			
			ofPushMatrix(); ofTranslate(pos);  ofRotateY(rot.y); ofRotateX(rot.x); ofRotateZ(rot.z);
			{
                ofSetColor(255);
				ofDrawSphere(0, 0, 0, 0.005);
                
                ofSetColor(150);
                
                ofDrawBitmapString(ofToString(i + 1), 0.0, 0.01, 0.0);
                ofDrawLine(ofVec3f(), ofVec3f(0, 0, 0.15));
			}
			ofPopMatrix();
			
			camSpline.addVertex(pos);
			
			// add graph
            float gx = gxStep * (i - (currentFrame - numSampleFrames));
			float vh = ofGetHeight();
            
            float yawY = 1.0 - ((f->rot.y - prevYaw) / yawScale + 0.5);
            float pitchY = 1.0 - ((f->rot.x - prevPitch) / pitchScale + 0.5);
            
			yawGraph.addVertex(gx, vh * yawY);
			pitchGraph.addVertex(gx, vh * pitchY);
			
		}
		
		ofSetColor(255);
		camSpline.draw();
		
	}
	if (showRawPose) { ofPopMatrix(); }
	
    if (enableAutoCam) {
        grabCam.end();
    } else {
        grabCam.end();
    }
	
	ofSetColor(0, 255, 0);
	yawGraph.draw();
	for (auto& pt : yawGraph.getVertices()) {
		ofDrawCircle(pt, 4);
	}
	
	ofSetColor(255, 0, 0);
	pitchGraph.draw();
	for (auto& pt : pitchGraph.getVertices()) {
		ofDrawCircle(pt, 4);
	}
	
	// visible?
	if (!trackerVisible) {
		ofSetColor(255, 0, 0, 128);
		ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());
	}
	
	ofPopStyle();
	
	drawImGui();
}

//--------------------------------------------------------------
void ofApp::drawImGui() {
	
	
	static bool calibPtChanged;
	calibPtChanged = false;
	
	
	gui.begin();
	
	ImGui::Text("Scene: %s", currentSceneName.c_str());
	ImGui::Text("Current Frame: %s", ofToString(currentFrame).c_str());
	
	
	ImGui::Checkbox("Show Raw Pose", &showRawPose);
    
    if (ImGui::Checkbox("Enable Auto Cam", &enableAutoCam)) {
        if (!enableAutoCam) {
            ofLogNotice() << "Disabled auto cam";
            grabCam.setTransformMatrix(autoCam.getGlobalTransformMatrix());
        }
    }
    
    ImGui::SliderFloat("Auto Cam Distance", &autoCamDistance, 0.1, 2.0);
    
    ImGui::SliderInt("Sample Frames", &numSampleFrames, 1, 60);
	
	
	if (ImGui::Button("Confirm Manually")) {
		
		sortCurrentScene();
	}
	
	if (ImGui::TreeNode("Calibration")) {
	
		if (ImGui::Button("Set Origin")) {
			
			calibOrigin = vtRawPose.getTranslation();
			calibPtChanged = true;
		}
		
		if (ImGui::Button("Set Axis X")) {
			
			calibAxisX = vtRawPose.getTranslation();
			calibPtChanged = true;
		}
		
		if (ImGui::Button("Set Alt")) {
			
			calibAlt = vtRawPose.getTranslation();
			calibPtChanged = true;
		}
		
		
		static ofMatrix4x4 vtOffset;
		
		if (calibPtChanged) {
			
			// init
			vtOffset.set(1, 0, 0, 0,
						 0, 1, 0, 0,
						 0, 0, 1, 0,
						 0, 0, 0, 1);
			
			ofVec3f axisX = (calibAxisX - calibOrigin).normalize();
			ofVec3f axisZ = (calibAlt - calibOrigin).normalize();
			ofVec3f axisY = axisZ.getCrossed(axisX).normalize();
			
			axisZ = axisX.getCrossed(axisY).normalize();
			
			vtOffset._mat[0].set(axisX);
			vtOffset._mat[1].set(axisY);
			vtOffset._mat[2].set(axisZ);
			
			vtOffset.setTranslation(calibOrigin);
			
			vtCalib.makeInvertOf(vtOffset);
		}
		
		if (ImGui::Button("Set Direction")) {
			
			ofMatrix4x4 vtDir, camDir, zInv;
			
			zInv.makeRotationMatrix(180, ofVec3f(0, 1, 0));
			
			vtDir.set(vtRawPose);
			
			camDir.makeLookAtMatrix(vtDir.getTranslation(),			// eye
									ofVec3f(),						// center
									-vtRawPose.getRowAsVec3f(2));	// up vector
			
			dirCalib = zInv * (camDir * vtDir.getInverse());
		}
		
		ImGui::TreePop();
	}
	
	
	gui.end();
}

//--------------------------------------------------------------
void ofApp::loadCurrentScene() {
		
	ofxAdvancedXmlSettings sceneSettings;
	
	bool exists = sceneSettings.load("scene_" + currentSceneName + ".xml");
	
	if (exists) {
		
		sheet.clear();
		
		sceneSettings.pushTag("sheet");
		{
			int numFrames = sceneSettings.getNumTags("frame");
			
			for (int i = 0; i < numFrames; i++) {
				
				sceneSettings.pushTag("frame", i);
				
				Frame *f = new Frame();
				f->empty	= sceneSettings.getValue("empty",	f->empty);
				f->pos		= sceneSettings.getValue("pos",		f->pos);
				f->rot		= sceneSettings.getValue("rot",		f->rot);
				f->hash		= sceneSettings.getValue("hash",	f->hash);
				sheet.push_back(f);
				
				sceneSettings.popTag();
			}
		}
		sceneSettings.popTag();
		
		sheetDirPath = sceneSettings.getValue("sheetDirPath", "");
	}
}

//--------------------------------------------------------------
void ofApp::saveCurrentScene() {
	
	// save Scene
	ofxAdvancedXmlSettings sceneSettings;
	
	sceneSettings.addPushTag("sheet");
	{
		for (auto& f : sheet) {
			
			int i = sceneSettings.addTag("frame");
			
			sceneSettings.pushTag("frame", i);
			{
				sceneSettings.setValue("empty", f->empty);
				sceneSettings.setValue("pos",	f->pos);
				sceneSettings.setValue("rot",	f->rot);
				sceneSettings.setValue("hash",	f->hash);
			}
			sceneSettings.popTag();
		}
	}
	sceneSettings.popTag();
	
	sceneSettings.setValue("sheetDirPath", sheetDirPath);
	
	sceneSettings.save("scene_" + currentSceneName + ".xml");
}

//--------------------------------------------------------------
void ofApp::sortCurrentScene() {
	
	ofDirectory sheetDir(sheetDirPath);
	
	if (!sheetDir.exists()) {
		return;
	}
	
	// TODO: Add RAW extensions
	sheetDir.allowExt("png");
	sheetDir.allowExt("jpg");
	sheetDir.allowExt("tiff");
	
	
	sheetDir.listDir();
	vector<ofFile> files = sheetDir.getFiles();
	
	// make a dict of hash with existing scene data
	map<string, Frame*> frameMap;
	
	for (auto& f : sheet) {
		if (!f->empty) {
			frameMap[f->hash] = f;
		}
	}
	
	// assign new frame
	sheet.clear();
	
	for (auto& file : files) {
		
		string baseName = file.getBaseName();
		string filePath = file.getAbsolutePath();
		
		string seqStr	= baseName.substr(baseName.length() - 4);
		int fnum		= ofToInt(seqStr);
		
		string hash		= ofGetFileHash(filePath);
		//ofLogNotice() << baseName << " -> " << fnum << " -> " << hash;
		
		while (sheet.size() < fnum - 1) {
			sheet.push_back(new Frame());
		}
		
		if (frameMap[hash]) {
			sheet.push_back(frameMap[hash]);
		} else {
			ofLogNotice() << "sortCurrentScene(): Invalid hash found.";
		}
		
		
	}
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){

}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ 

}
