/////////////////////////////////////////
//
//                  OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Viewport class
// Created 22/1/02
// Jason Boettcher

#include <iostream>

#include "LieroX.h"
#include "debug.h"
#include "CClient.h"
#include "Options.h" // for controls_t
#include "CWorm.h"
#include "MathLib.h"

using namespace std;


///////////////////
// Setup the viewport
void CViewport::Setup(int l, int t, int vw, int vh, int type)
{
	Left = l;
	Top = t;
	VirtWidth = vw;
	VirtHeight = vh;

	Width = vw/2;
	Height = vh/2;

    pcTargetWorm = NULL;
    nType = type;
	bSmooth = false;
}


///////////////////
// Setup the keyboard inputs for freelook
void CViewport::setupInputs(const controls_t& Inputs)
{
	cUp = new CInput();
	cDown = new CInput();
	cLeft = new CInput();
	cRight = new CInput();
    
    cUp.get()->Setup(		Inputs[SIN_UP] );
	cDown.get()->Setup(	Inputs[SIN_DOWN] );
	cLeft.get()->Setup(	Inputs[SIN_LEFT] );
	cRight.get()->Setup(	Inputs[SIN_RIGHT] );
}


///////////////////
// Process a viewport
void CViewport::Process(CWorm *pcWormList, CViewport *pcViewList, int MWidth, int MHeight, int iGameMode)
{
    int hx = Width/2;
	int hy = Height/2;

    // Follow a player
    switch (nType)  {
	case VW_FOLLOW: {
	    // The player will ideally be in the centre of the viewport
	    // The viewport will then rest against the edges if that cannot happen

        // If we have no target at all, find one
        if( !pcTargetWorm ) {
			cout << "find new worm for viewport because we have currently none" << endl;
            
            // Try and find a living worm first
            CWorm *t = findTarget(pcWormList, pcViewList, true);

            // If no living worms, try a dead worm (but still in the game)
            if(!t)
                t = findTarget(pcWormList, pcViewList, false);

            if(t) {
                pcTargetWorm = t;
                fTimer = -1;
            } else {
                // If we didn't find a new worm, go into freelook mode                    
                pcTargetWorm = NULL;
                nType = VW_FREELOOK;
                fTimer = -1;
                return;
            }
        }


        // Check if the target is out of the game
        if( pcTargetWorm ) {
            if( pcTargetWorm->getLives() == WRM_OUT ) {
                // Setup the timer to wait 2.5 seconds before changing targets
                if( fTimer == -1 )
                    fTimer = tLX->fCurTime + 2.5f;

                // Time up? Change targets
                if( tLX->fCurTime > fTimer ) {
					cout << "find new worm for viewport because current is out of the game" << endl;
					
                    // Try and find a living worm first
                    CWorm *t = findTarget(pcWormList, pcViewList, true);

                    // If no living worms, try a dead worm (but still in the game)
                    if(!t)
                        t = findTarget(pcWormList, pcViewList, false);

                    if(t) {
                        pcTargetWorm = t;
                        fTimer = -1;
                    } else {

                        // If we didn't find a new worm, go into freelook mode                    
                        pcTargetWorm = NULL;
                        nType = VW_FREELOOK;
                        fTimer = -1;
                        return;
                    }
                }
            }
        }
	    
        // Follow the worm

        if( pcTargetWorm ) {
            if( pcTargetWorm->getAlive() ) {
				if( bSmooth )
					setSmoothPosition( pcTargetWorm->getPos().x-hx, pcTargetWorm->getPos().y-hy, tLX->fDeltaTime );
				else
				{
					WorldX = (int)(pcTargetWorm->getPos().x-hx);
					WorldY = (int)(pcTargetWorm->getPos().y-hy);									
				};

                // Clear the timer
                fTimer = -1;
            } /*else
            	cout << "viewport: our worm is dead" << endl; */
        }
    }
	break; // VW_FOLLOW

    // Cycle
    case VW_CYCLE: {
        // Cycles through players. If a player dies (but not necessarily out), move onto another living player

        // Check if the target is out of the game or has died
        if( pcTargetWorm ) {
			if( pcTargetWorm->getLives() == WRM_OUT )
				cout << "find new worm for viewport because current is out of game" << endl;

			if( pcTargetWorm->getLives() == WRM_OUT || !pcTargetWorm->getAlive() ) {
                // Setup the timer to wait 0.5 seconds before changing targets
                if( fTimer == -1 )
                    fTimer = tLX->fCurTime + 0.5f;

                // Time up? Change targets
                if( tLX->fCurTime > fTimer ) {

                    // Try and find a living worm first
                    CWorm *t = findTarget(pcWormList, pcViewList, true);

                    // If no living worms, try a dead worm (but still in the game)
                    if(!t)
                        t = findTarget(pcWormList, pcViewList, false);

                    if(t) {
                        pcTargetWorm = t;
                        fTimer = -1;
                    } else {
                        // If we didn't find a new worm, go into freelook mode
                        pcTargetWorm = NULL;
                        nType = VW_FREELOOK;
                        fTimer = -1;
						cout << "no worm found, going into freelook mode" << endl;
						return;
                    }
                }
            }
        }

        // Follow the worm
        if( pcTargetWorm ) {
            if( pcTargetWorm->getAlive() ) {
				if( bSmooth )
					setSmoothPosition( pcTargetWorm->getPos().x-hx, pcTargetWorm->getPos().y-hy, tLX->fDeltaTime );
				else
				{
					WorldX = (int)(pcTargetWorm->getPos().x-hx);
					WorldY = (int)(pcTargetWorm->getPos().y-hy);
				};

                // Clear the timer
                fTimer = -1;
            }
        }
    }
	break; // VW_CYCLE


    // Action
	case VW_ACTIONCAM: {
        short i,j;
        // Finds a group of worms and smoothly moves around to show the whole group of players in a fight

        pcTargetWorm = NULL;
        
        // Generate a score for each worm depending on closeness of other worms
        // The worm with the lowest score is used as a focus point        
        float fScores[MAX_WORMS];
        for(i=0; i<MAX_WORMS; i++)
            fScores[i] = -1;
        
        // Go through all permutations of the worms
        for(i=0; i<MAX_WORMS; i++) {
            if( !pcWormList[i].isUsed() || !pcWormList[i].getAlive() || pcWormList[i].getLives() == WRM_OUT )
                continue;
            
            // Set a zero score because we are at least alive and well
            fScores[i] = 0;
            
            for(j=0; j<MAX_WORMS; j++) {
                if( !pcWormList[j].isUsed() || !pcWormList[j].getAlive() || pcWormList[j].getLives() == WRM_OUT )
                    continue;
                
                fScores[i] += CalculateDistance(pcWormList[i].getPos(), pcWormList[j].getPos());
            }
        }
        
        CWorm *pcFocus = NULL;
        float lowest = 99999;
        
        for(i=0; i<MAX_WORMS; i++) {
            if( !pcWormList[i].isUsed() || !pcWormList[i].getAlive() || pcWormList[i].getLives() == WRM_OUT )
                continue;
            
            if( fScores[i] < lowest ) {
                pcFocus = &pcWormList[i];
                lowest = fScores[i];
            }
        }
        
        // No focus point? Leave
        if( !pcFocus )
            return;
        
        // The focus worm is our target
        tgtPos = pcFocus->getPos();
        
        CVec dir = tgtPos - curPos;
        float l = NormalizeVector(&dir);
        
        float speed = l*2.5f;
        speed = MIN((float)300,speed);
        
        if( l > 2 )
            curPos += dir*speed * tLX->fDeltaTime;
        
        WorldX = (int)floor(curPos.x-hx);
        WorldY = Round(curPos.y-hy);
    }
	break; // VW_ACTIONCAM


    // Free look
    case VW_FREELOOK: {
        float scrollSpeed = 300.0f*tLX->fDeltaTime;

        // Uses the players keys to scroll around
        if( cUp.get()->isDown() )
            curPos -= CVec(0,scrollSpeed);
        else if( cDown.get()->isDown() )
            curPos += CVec(0,scrollSpeed);
        if( cLeft.get()->isDown() )
            curPos -= CVec(scrollSpeed,0);
        else if( cRight.get()->isDown() )
            curPos += CVec(scrollSpeed,0);

        // Clamp our movement
        curPos.x=( MAX((float)0,curPos.x) );
        curPos.y=( MAX((float)0,curPos.y) );
        curPos.x=( MIN((float)MWidth-Width,curPos.x) );
        curPos.y=( MIN((float)MHeight-Height,curPos.y) );
        
        WorldX = (int)curPos.x;
        WorldY = (int)curPos.y;
    }
	break;

	}	// switch


	// Shake the viewport a bit
	if(bShaking) {
		if(tLX->fCurTime - fShakestart > 0.2f) {
			bShaking = false;
			iShakeAmount = 0;
		}
		else {

            // Don't shake the action/freelook cam
            if( nType != VW_ACTIONCAM && nType != VW_FREELOOK ) {

                // Clamp it to the edges, then shake. So we can still see shaking near edges
                Clamp(MWidth, MHeight);

			    // Shake
			    WorldX += (int)(GetRandomNum() * (float)iShakeAmount);
			    WorldY += (int)(GetRandomNum() * (float)iShakeAmount);
            }
		}
	}

	// Clamp it
	Clamp(MWidth, MHeight);
}


///////////////////
// Find a target worm
CWorm *CViewport::findTarget(CWorm *pcWormList, CViewport *pcViewList, bool bAlive)
{
    // Find a worm that isn't already a target by another viewport
	short viewcount;
	short v;
	CWorm *t = NULL;
    for( short w=0; w<MAX_WORMS; w++ ) {
        if( !pcWormList[w].isUsed() )
            continue;
        if( pcWormList[w].getLives() == WRM_OUT )
            continue;

        // If the worm isn't alive, and we want a living worm, skip the worm
        if( !pcWormList[w].getAlive() && bAlive )
            continue;

        viewcount = 0;
        for( v=0; v<NUM_VIEWPORTS; v++ ) {

            // Make sure this isn't our viewport
            if( pcViewList[v].nID == nID )
                continue;

            t = pcViewList[v].getTarget();
            if(t) {
                if( pcWormList[w].getID() == t->getID() )
                    viewcount++;
            }
        }

        // If this worm was in none of the viewports, use the worm
        if( !viewcount )
            return &pcWormList[w];
    }

    // No good target
    return NULL;
}


///////////////////
// Resets the viewport simulations (timer, movement, etc)
void CViewport::reset(void)
{
    fTimer = -1;
}


///////////////////
// Clamp the viewport if it exceeds any boundaries
void CViewport::Clamp(int MWidth, int MHeight)
{
	WorldX = MAX(0,WorldX);
	WorldY = MAX(0,WorldY);

	WorldX = MIN(WorldX,MWidth-Width);
	WorldY = MIN(WorldY,MHeight-Height);
}


///////////////////
// Clamp the viewport if it exceeds any boundaries (for filtered drawing)
void CViewport::ClampFiltered(int MWidth, int MHeight)
{
	// Note: Filtered drawing of the map blurs between pixels so we need to clamp the max size by 2
	WorldX = MAX(0,WorldX);
	WorldY = MAX(0,WorldY);

	WorldX = MIN(WorldX,MWidth-Width-2);
	WorldY = MIN(WorldY,MHeight-Height-2);
}


///////////////////
// Get the rectangle of the viewport
SDL_Rect CViewport::getRect(void)
{
	static SDL_Rect r;

	r.x = Left;
	r.y = Top;
	r.w = VirtWidth;
	r.h = VirtHeight;

	return r;
}


///////////////////
// Shake the viewport
void CViewport::Shake(int amount)
{
	if( bSmooth )
		return;	// Don't shake viewport if we're spectating - it's just hurting my eyes.
	fShakestart = tLX->fCurTime;
	bShaking = true;
	if(amount > iShakeAmount)
		iShakeAmount = amount;
}


///////////////////
// Check if a point is inside this viewport
bool CViewport::inView(CVec pos)
{
	return	(int)pos.x >= WorldX &&
			(int)pos.y >= WorldY &&
			(int)pos.x <= WorldX+Width &&
			(int)pos.y <= WorldY+Height;
}

void CViewport::setSmooth(bool _b)	
{
	bSmooth = _b; 
	cSmoothVel = cSmoothAccel = CVec(0,0); 
	curPos = tgtPos = CVec( (float)WorldX, (float)WorldY ); 
}

// Constants that control behavior of smoothed viewport
/* // Nice smoothy settings
const float fVelMax = 1200.0f, fAccelMax = 400.0f, 
			fVelIncrease = 20.0f, fAccelIncrease = 70.0f,
			fVelDecay = 10.0f, fAccelDecay = 30.0f;
*/
// Hard follow settings, only slightly smoothed
const float fVelMax = 2000.0f, fAccelMax = 600.0f, 
			fVelIncrease = 100.0f, fAccelIncrease = 200.0f,
			fVelDecay = 20.0f, fAccelDecay = 50.0f;

void CViewport::setSmoothPosition( float X, float Y, float DeltaTime )
{
	// TODO: these formulas work only for FPS > 50, so we'll run simulation twice on low FPS
	if( DeltaTime > 0.015f )
	{
		setSmoothPosition( X, Y, 0.015f );
		setSmoothPosition( X, Y, DeltaTime - 0.015f );
		return;
	};

	CVec Coords( X, Y );
	CVec Diff = Coords - curPos;

	cSmoothAccel += Diff * ( fAccelIncrease * DeltaTime );
	cSmoothAccel -= cSmoothAccel * fAccelDecay * DeltaTime;
	if( cSmoothAccel.GetLength2() > fAccelMax*fAccelMax )
		cSmoothAccel *= fAccelMax / cSmoothAccel.GetLength();
	
	cSmoothVel += cSmoothAccel * ( fVelIncrease * DeltaTime );
	cSmoothVel -= cSmoothVel * fVelDecay * DeltaTime;
	if( cSmoothVel.GetLength2() > fVelMax*fVelMax )
		cSmoothVel *= fVelMax / cSmoothVel.GetLength();
	
	curPos += cSmoothVel * DeltaTime;

	WorldX = (int)curPos.x;
	WorldY = (int)curPos.y;
};

