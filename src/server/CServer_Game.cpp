/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Server class - Game routines
// Created 11/7/02
// Jason Boettcher


#include "LieroX.h"
#include "CServer.h"
#include "CClient.h"
#include "StringUtils.h"
#include "Protocol.h"
#include "CWorm.h"
#include "MathLib.h"


///////////////////
// Spawn a worm
void GameServer::SpawnWorm(CWorm *Worm)
{
	if (iGameOver)
		return;

	CVec pos = FindSpot();

	Worm->Spawn(pos);

	// Send a spawn packet to everyone
	static CBytestream bs;
	bs.Clear();
	bs.writeByte(S2C_SPAWNWORM);
	bs.writeInt(Worm->getID(), 1);
	bs.writeInt( (int)pos.x, 2);
	bs.writeInt( (int)pos.y, 2);
	SendGlobalPacket(&bs);
}


///////////////////
// Spawn a worm
void GameServer::SpawnWorm(CWorm *Worm, CVec pos, CClient *cl)
{
	if (iGameOver)
		return;

	Worm->Spawn(pos);

	// Send a spawn packet to everyone
	static CBytestream bs;
	bs.Clear();
	bs.writeByte(S2C_SPAWNWORM);
	bs.writeInt(Worm->getID(), 1);
	bs.writeInt( (int)pos.x, 2);
	bs.writeInt( (int)pos.y, 2);
	SendPacket(&bs, cl);
}


///////////////////
// Find a spot with no rock
CVec GameServer::FindSpot(void)
{
    int     x, y;
    int     px, py;
    bool    first = true;
    int     cols = cMap->getGridCols() - 1;       // Note: -1 because the grid is slightly larger than the
    int     rows = cMap->getGridRows() - 1;       // level size
    int     gw = cMap->getGridWidth();
    int     gh = cMap->getGridHeight();


    // Find a random cell to start in
    px = (int)(fabs(GetRandomNum()) * (float)cols);
	py = (int)(fabs(GetRandomNum()) * (float)rows);

    x = px; y = py;

    // Start from the cell and go through until we get to an empty cell
	uchar pf;
    while(true) {
        while(true) {
            // If we're on the original starting cell, and it's not the first move we have checked all cells
            // and should leave
            if(!first) {
                if(px == x && py == y) {
                    return CVec((float)x * gw + gw / 2, (float)y * gh + gh / 2);
                }
            }
            first = false;

            pf = *(cMap->getGridFlags() + y*cMap->getGridCols() + x);
            if(!(pf & PX_ROCK))
                return CVec((float)x * gw + gw / 2, (float)y * gh + gh / 2);

            if(++x >= cols) {
                x = 0;
                break;
            }
        }

        if(++y >= rows) {
            y = 0;
            x = 0;
        }
    }

    // Can't get here
    return CVec((float)x, (float)y);
}


///////////////////
// Simulate the game stuff
void GameServer::SimulateGame(void)
{
	if(iState != SVS_PLAYING)
		return;

	// If this is a remote game, and game over,
	// and we've seen the scoreboard for a certain amount of time, go back to the lobby
	if(iGameOver && tLX->fCurTime - fGameOverTime > LX_ENDWAIT && iState != SVS_LOBBY && tGameInfo.iGameType == GME_HOST) {
		gotoLobby();
		return;
	}

	// Process worms
	CWorm *w = cWorms;
	short i;
	for(i=0;i<MAX_WORMS;i++,w++) {
		if(!w->isUsed())
			continue;

		if(!w->getAlive() && w->getLives() != WRM_OUT) {
			// Check to see if they have been dead for longer then 2.5 seconds
			if(tLX->fCurTime - w->getTimeofDeath() > 2.5)
				SpawnWorm(w);
		}

		// Add their time in a game of tag
		if(iGameType == GMT_TAG && w->getTagIT() && w->getAlive())  {
			w->incrementTagTime(tLX->fRealDeltaTime);

			// Log
			log_worm_t *logworm = GetLogWorm(w->getID());
			if (logworm)
				logworm->fTagTime += tLX->fRealDeltaTime;
		}

		// Simulate the worm's weapons
		w->SimulateWeapon( tLX->fRealDeltaTime );
	}



	// Check if any bonuses have been in for too long and need to be destroyed
	if (iBonusesOn)  {
		for(i=0; i<MAX_BONUSES; i++) {
			if(!cBonuses[i].getUsed())
				continue;

			// If it's been here too long, destroy it
			if( tLX->fCurTime - cBonuses[i].getSpawnTime() > tLXOptions->tGameinfo.fBonusLife ) {
				static CBytestream bs;
				bs.Clear();
				cBonuses[i].setUsed(false);

				bs.writeByte(S2C_DESTROYBONUS);
				bs.writeByte((byte)i);
				SendGlobalPacket(&bs);
			}
		}
	}



	// Check if we need to spawn a bonus
	if(tLX->fCurTime - fLastBonusTime > tLXOptions->tGameinfo.fBonusFreq && iBonusesOn && !iGameOver) {

		SpawnBonus();

		fLastBonusTime = tLX->fCurTime;
	}
}


///////////////////
// Spawn a bonus
void GameServer::SpawnBonus(void)
{
	// Find an empty spot for the bonus
	CVec pos = FindSpot();

	// Carve a hole for it
	//cMap->CarveHole(SPAWN_HOLESIZE,pos);

	// NOTE: Increase to 2 when we want to use the fullcharge bonus
	int type = GetRandomInt(1);
	int wpn = GetRandomInt(cGameScript.GetNumWeapons()-1);


	// Find a free bonus spot
	CBonus *b = cBonuses;
	int spot = -1;
	for(short i=0;i<MAX_BONUSES;i++,b++) {
		if(b->getUsed())
			continue;

		spot = i;
		break;
	}

	// No spots
	if(spot == -1)
		return;


    // Check if the weapon chosen is enabled or a 'bonus' weapon in the restrictions
    int orig = wpn;
    while(1) {
        int state = cWeaponRestrictions.getWeaponState( (cGameScript.GetWeapons()+wpn)->Name );

        if( state != wpr_banned )
            break;

        wpn++;
        if( wpn >= cGameScript.GetNumWeapons()-1 )
            wpn=0;

        // No good weapons? Just leave with original choice
        if( wpn == orig )
            break;
    }


	b->Spawn(pos, type, wpn, &cGameScript);


	// Send the spawn the everyone
	CBytestream bs;

	bs.writeByte(S2C_SPAWNBONUS);
	bs.writeInt(type,1);
	if(type == BNS_WEAPON)
		bs.writeInt(wpn,1);
	bs.writeInt(spot,1);

	bs.writeInt((int)pos.x, 2);
	bs.writeInt((int)pos.y, 2);

	SendGlobalPacket(&bs);
}


///////////////////
// Tag a worm
void GameServer::TagWorm(int id)
{
	// Safety check
	if(id < 0 || id >= MAX_WORMS || getState() != SVS_PLAYING)
		return;

	CWorm *w = &cWorms[id];

	short i;

	// Go through all the worms, setting their tag to false
	for(i=0;i<MAX_WORMS;i++) {
		if(cWorms[i].isUsed())  {
			cWorms[i].setTagIT(false);
		}
	}

	// Log this
	if (tGameLog)
		if (tGameLog->tWorms)  {
			for (i=0;i<tGameLog->iNumWorms;i++)  {
				if (tGameLog->tWorms[i].iID == id)
					tGameLog->tWorms[i].bTagIT = true;
				else
					tGameLog->tWorms[i].bTagIT = false;
			}
		}


	w->setTagIT(true);

	// Let everyone know this guy is tagged
	CBytestream bs;
	bs.writeByte(S2C_TAGUPDATE);
	bs.writeInt(id,1);
	bs.writeFloat(w->getTagTime());

	SendGlobalPacket(&bs);

	//Take care of the <none> tag
	if (networkTexts->sWormIsIt != "<none>")  {
		SendGlobalText(OldLxCompatibleString(replacemax(networkTexts->sWormIsIt,"<player>",w->getName(),1)),
						TXT_NORMAL);
	}
}


///////////////////
// Tag a random worm
void GameServer::TagRandomWorm(void)
{
	float time = 99999;
	std::vector<int> all_lowest;


	// Go through finding the worm with the lowest tag time
	// A bit more fairer then random picking
	unsigned short i;
	for(i=0;i<MAX_WORMS;i++) {
		if(cWorms[i].isUsed() && cWorms[i].getLives() != WRM_OUT) {
			if(cWorms[i].getTagTime() < time) {
				time = cWorms[i].getTagTime();
			}
		}
	}

	// Find all the worms that have the lowest time
	for (i=0;i<MAX_WORMS;i++)  {
		if (cWorms[i].isUsed() && cWorms[i].getLives() != WRM_OUT)  {
			if (cWorms[i].getTagTime() == time)  {
				all_lowest.push_back(i);
			}
		}
	}

	// Choose a random worm from all those having the lowest time
	int random_lowest = GetRandomInt(all_lowest.size()-1);


	// Tag the lowest tagged worm
	TagWorm(all_lowest[random_lowest]);



	// Go through the worms finding the tagged worm
	/*int i,n=0;
	for(i=0;i<MAX_WORMS;i++) {
		if(cWorms[i].isUsed()) {
			if(n==tag)
				tag=i;
			n++;
		}
	}*/

	// Tag the player
	//TagWorm(tag);
}


///////////////////
// Worm is shooting
void GameServer::WormShoot(CWorm *w)
{
	// If the worm is a VIP (yellow team) and the gametype is VIP don't shoot
	if(w->getVIP() && iGameType == GMT_VIP)
		return;

	wpnslot_t *Slot = w->getCurWeapon();

	if(Slot->Reloading)
		return;

	if(Slot->LastFire>0)
		return;

	// Don't shoot with banned weapons
	if (!Slot->Enabled)
		return;

	if(!Slot->Weapon) {
		printf("WARNING: trying to shoot with an unitialized weapon!");
		return;
	}

	Slot->LastFire = Slot->Weapon->ROF;


	// Beam weapons get processed differently
	if(Slot->Weapon->Type == WPN_BEAM) {
		ShootBeam(w);
		return;
	}

	// Must be a projectile
	if(Slot->Weapon->Type != WPN_PROJECTILE)
		return;

	// Get the direction angle
	float Angle = w->getAngle();
	if(w->getDirection() == DIR_LEFT)
		Angle=180-Angle;

	static CVec sprd;


	static CVec dir;
	GetAngles((int)Angle,&dir,NULL);
	CVec pos = w->getPos();// + dir*6;

	// Add the shot to the shooting list
	dir = *w->getVelocity();
	float speed = NormalizeVector( &dir );
	//Angle = (float)-atan2(dir.x,dir.y) * (180.0f/PI);
	//Angle += GetRandomNum() * (float)Slot->Weapon->ProjSpread;


	if(Angle < 0)
		Angle+=360;
	if(Angle > 360)
		Angle-=360;
	if(Angle == 360)
		Angle=0;

	// Add the shot to ALL the connected clients shootlist
	CClient *cl = cClients;
	for(short i=0; i<MAX_CLIENTS; i++,cl++) {
		if(cl->getStatus() == NET_DISCONNECTED)
			continue;

		cl->getShootList()->addShoot( fServertime, speed, (int)Angle, w);
	}


	//
	// Note: Drain does NOT have to use a delta time, because shoot timing is controlled by the ROF
	//

	// Drain the Weapon charge
	Slot->Charge -= Slot->Weapon->Drain / 100;
	if(Slot->Charge <= 0) {
		Slot->Charge = 0;
		Slot->Reloading = true;
	}
}


///////////////////
// Worm is shooting a beam
void GameServer::ShootBeam(CWorm *w)
{
	wpnslot_t *Slot = w->getCurWeapon();

	// Get the direction angle
	float Angle = w->getAngle();
	if(w->getDirection() == DIR_LEFT)
		Angle=180-Angle;

	if(Angle < 0)
		Angle+=360;
	if(Angle > 360)
		Angle-=360;
	if(Angle == 360)
		Angle=0;

	// Add the shot to ALL the connected clients shootlist
	CClient *cl = cClients;
	for(int i=0; i<MAX_CLIENTS; i++,cl++) {
		if(cl->getStatus() == NET_DISCONNECTED)
			continue;

		cl->getShootList()->addShoot( fServertime, 0, (int)Angle, w);
	}


	// Drain the Weapon charge
	Slot->Charge -= Slot->Weapon->Drain / 100;
	if(Slot->Charge < 0) {
		Slot->Charge = 0;
		Slot->Reloading = true;
	}
}


///////////////////
// Go back to the lobby
void GameServer::gotoLobby(void)
{
	// Tell all the clients
	CBytestream bs;
	bs.writeByte(S2C_GOTOLOBBY);
	SendGlobalPacket(&bs);

	// Shutdown the log
	ShutdownLog();

	// Clear the info
	iState = SVS_LOBBY;

	short i;
	for(i=0;i<MAX_WORMS;i++) {
		if(cWorms[i].isUsed()) {
			cWorms[i].getLobby()->iReady = false;
			cWorms[i].setGameReady(false);
			cWorms[i].setTagIT(false);
			cWorms[i].setTagTime(0);
		}
	}

	for(i=0; i<MAX_CLIENTS; i++) {
		cClients[i].setGameReady(false);
	}

	fLastUpdateSent = -9999;
	fGameOverTime = -9999;

	SendWormLobbyUpdate();


	// Shutdown the game items
	cGameScript.Shutdown();

	bTakeScreenshot = false;
}


///////////////////
// Send out a game over for demolitions mode
// Called by client (for local games only!)
void GameServer::DemolitionsGameOver(int winner)
{
    CBytestream bs;

    // Let everyone know that the game is over
	bs.writeByte(S2C_GAMEOVER);
	bs.writeInt(winner,1);

	// Game over
	iGameOver = true;
	fGameOverTime = tLX->fCurTime;

    SendGlobalPacket(&bs);
}


///////////////////
// Recheck the game status
// Called when a player has left the game (for various reasons)
void GameServer::RecheckGame(void)
{
    short i;

    //
    // If this is a tag game, make sure a person is tagged
    // If not, chances are the tagged person left
    //
    if( iGameType == GMT_TAG && getState() == SVS_PLAYING ) {
        int tag = -1;

        CWorm *w = cWorms;
        for(i=0; i<MAX_WORMS; i++, w++) {
            if( !w->isUsed() )
                continue;

            if( w->getTagIT() )
                tag = i;
        }

        // Anyone tagged?
        // If not, tag a new person
        if( tag == -1 )
            TagRandomWorm();
    }

	// Check how many worms are alive
	if (getState() == SVS_PLAYING && !iGameOver)  {
		CWorm *w = cWorms;
		short wormcount = 0;
		short wormid = 0;
		for(i=0; i<MAX_WORMS; i++, w++)
			if (w->isUsed() && w->getLives() != WRM_OUT)  {
				wormcount++;
				wormid = i; // Save the worm id
			}

		if (!iGameOver)  {

			//static char buf[256];
			bool EndGame = false;

			//
			// TEAM DEATHMATCH: Declare the team as winner
			//
			switch (iGameType)  {
			case  GMT_TEAMDEATH:  {
				const std::string TeamNames[] = {"blue", "red", "green", "yellow"};
				int TeamCount[4];

				w = cWorms;

				for(i=0;i<4;i++)
					TeamCount[i]=0;

				// Check if anyone else is left on the team
				for(i=0;i<MAX_WORMS;i++,w++) {
					if(w->isUsed() && w->getLives() != WRM_OUT)
							TeamCount[w->getTeam()]++;
				}

				short teamsleft = 0;
				short team = 0;

				// Get the number of teams left
				for(i=0;i<4;i++)
					if(TeamCount[i])  {
						teamsleft++;
						team = i;
					}

				// Send the text
				if (teamsleft <= 1)  {
					if (networkTexts->sTeamHasWon != "<none>")  {
						SendGlobalText(OldLxCompatibleString(replacemax(networkTexts->sTeamHasWon,"<team>",TeamNames[team],1)),
										TXT_NORMAL);
					}
					EndGame = true;
				}
			}
			break; // TEAMDEATH

			//
			// DEATHMATCH: Declare the player who is alive as winner
			//
			case GMT_DEATHMATCH:  {
				// Only one worm left, end the game
				if (wormcount < 2)  {
					// Get the worm that is still alive and declare him as winner
					w = cWorms + wormid;

					// Send the text
					if (networkTexts->sPlayerHasWon != "<none>")  {
						SendGlobalText(OldLxCompatibleString(replacemax(networkTexts->sPlayerHasWon,"<player>",w->getName(),1)),
										TXT_NORMAL);
					}
					EndGame = true;
				}
			}
			break; // DEATHMATCH

			//
			// TAG: Declare the worm with greatest tag time as winner
			//
			case GMT_TAG:  {
				// Only one worm left, end the game
				if (wormcount < 2)  {
					// Get the worm with greatest tag time
					float fMaxTagTime = 0;
					int worm = 0;
					w = cWorms;
					for (int i=0;i<MAX_WORMS;i++)
						if (w->isUsed())
							if (w->getTagTime() > fMaxTagTime)  {
								fMaxTagTime = w->getTagTime();
								worm = i;
							}

					// Worm with greatest tag time
					w = cWorms+worm;

					// Send the text
					if (networkTexts->sPlayerHasWon!="<none>")  {
						SendGlobalText(OldLxCompatibleString(replacemax(networkTexts->sPlayerHasWon,"<player>",w->getName(),1)),
										TXT_NORMAL);
					}

					EndGame = true;
				}
			}

			break;  // TAG

			//
			// VIP: Declare the wining team to be the one that killed the VIP or the VIP depending on who is alive
			case GMT_VIP:  {
				const std::string TeamNames[] = {"VIP Defenders", "VIP Attackers", "VIP", "yellow"};
				int TeamCount[4];

				w = cWorms;

				for(i=0;i<4;i++)
					TeamCount[i]=0;

				short VIPs = 0;

				// Check if anyone else is left on the team
				for(i=0;i<MAX_WORMS;i++,w++) {
					if(w->isUsed() && w->getLives() != WRM_OUT) {
							TeamCount[w->getTeam()]++;
							VIPs += w->getVIP();
					}
				}

				short teamsleft = 0;
				short team = 0;

				// Get the number of teams left
				for(i=0;i<2;i++)
					if(TeamCount[i])  {
						teamsleft++;
						team = i;
					}

				// Send the text
				if (teamsleft <= 1)  {
					if (networkTexts->sTeamHasWon != "<none>")  {
						SendGlobalText(OldLxCompatibleString(replacemax(networkTexts->sTeamHasWon,"<team>",TeamNames[team],1)),
										TXT_NORMAL);
					}
					EndGame = true;
				}

				if(VIPs == 0) { // All the VIPs are out of the game
					team = 1; // VIP Attackers win
					if (networkTexts->sTeamHasWon != "<none>")  {
						SendGlobalText(OldLxCompatibleString(replacemax(networkTexts->sTeamHasWon,"<team>",TeamNames[team],1)),
										TXT_NORMAL);
					}
					EndGame = true;
				}
			}
			break; // VIP
			
			}

			// End the game
			if (EndGame)  {
				CBytestream bs;
				bs.Clear();
				bs.writeByte(S2C_GAMEOVER);
				bs.writeInt(wormid,1);

				iGameOver = true;
				fGameOverTime = tLX->fCurTime;

				// Send the Game Over
				SendGlobalPacket(&bs);
			}
		}
	}


}


///////////////////
// Checks if all the clients are ready to play
void GameServer::CheckReadyClient(void)
{
    short c;

    if(iState != SVS_GAME)
        return;

	bool allready = true;
	CClient *client = cClients;
	for(c=0; c<MAX_CLIENTS; c++, client++) {
		if(client->getStatus() == NET_DISCONNECTED || client->getStatus() == NET_ZOMBIE)
			continue;

		if(!client->getGameReady())
			allready = false;
	}

	// All ready to go?
	if(allready)
		BeginMatch();
}
