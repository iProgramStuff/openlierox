/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Checkbox
// Created 30/3/03
// Jason Boettcher


#include "LieroX.h"
#include "Menu.h"
#include "GfxPrimitives.h"
#include "StringUtils.h"
#include "CCheckbox.h"


///////////////////
// Draw the checkbox
void CCheckbox::Draw(SDL_Surface * bmpDest)
{
	if (bRedrawMenu)
		Menu_redrawBufferRect( iX,iY, 17,17 );

    if(bValue)
		DrawImageAdv(bmpDest, bmpImage, 17,0,iX,iY,17,17);
	else
	    DrawImageAdv(bmpDest, bmpImage, 0,0,iX,iY,17,17);
}


///////////////////
// Create
void CCheckbox::Create(void)
{
    bmpImage = LoadGameImage("data/frontend/checkbox.png");
}

///////////////////
// Load the style
void CCheckbox::LoadStyle(void/*node_t *cssNode*/)
{
	node_t *cssNode = NULL;
	// Find the default checkbox class, if none specified
	if (!cssNode) {
		cssNode = cWidgetStyles.FindNode("checkbox");
		if (!cssNode)
			return;
	}

	// Read properties
	property_t *prop = cssNode->tProperties;
	for(;prop;prop=prop->tNext) {
		// Image
		if (!stringcasecmp(prop->sName,"image"))  {
			bmpImage = LoadGameImage(prop->sValue);
		}
		// Image width
		if (!stringcasecmp(prop->sName,"image-width"))  {
			//iImgWidth = atoi(prop->sValue);
		}
		// Unknown
		else {
			printf("Warning: Unknown property %s in main Checkbox class", prop->sName.c_str());
		}
	}
}

static bool CCheckBox_WidgetRegistered = 
	CGuiSkin::RegisterWidget( "checkbox", & CCheckbox::WidgetCreator )
							( "var", CScriptableVars::SVT_STRING )
							( "click", CScriptableVars::SVT_STRING );
