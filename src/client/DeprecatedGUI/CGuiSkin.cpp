/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////

#include "LieroX.h"
#include "debug.h"

#include "DeprecatedGUI/CGuiSkin.h"
#include "DeprecatedGUI/CGuiSkinnedLayout.h"
#include "DeprecatedGUI/CWidget.h"
#include "DeprecatedGUI/CWidgetList.h"

#include "AuxLib.h"
#include "DeprecatedGUI/Menu.h"
#include "StringUtils.h"
#include "Cursor.h"

#include <iostream>
#include <sstream>
// XML parsing library
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>


namespace DeprecatedGUI {

CGuiSkin * CGuiSkin::m_instance = NULL;

CGuiSkin & CGuiSkin::Init()
{
	if( m_instance == NULL )
		m_instance = new CGuiSkin;
	return *m_instance;
};

void CGuiSkin::DeInit()
{

	if( CGuiSkin::m_instance != NULL )
	{
		CGuiSkin::ClearLayouts();
		delete CGuiSkin::m_instance;
		CGuiSkin::m_instance = NULL;
	};
};

std::string CGuiSkin::DumpWidgets()
{
	Init();
	std::ostringstream ret;
	for( std::map< std::string, std::pair< paramListVector_t, WidgetCreator_t > > :: iterator it =
			m_instance->m_widgets.begin();	it != m_instance->m_widgets.end(); it++ )
	{
		ret << it->first + "( ";
		bool first = true;
		for( paramListVector_t::const_iterator f = it->second.first.begin(); f != it->second.first.end(); ++f )
		{
			if( !first ) ret << ", ";
			switch( f->second )
			{
				case CScriptableVars::SVT_BOOL: ret << "bool"; break;
				case CScriptableVars::SVT_INT: ret << "int"; break;
				case CScriptableVars::SVT_FLOAT: ret << "float"; break;
				case CScriptableVars::SVT_STRING: ret << "string"; break;
				case CScriptableVars::SVT_COLOR: ret << "color"; break;
				case CScriptableVars::SVT_CALLBACK: ret << "callback"; break;
			};
			ret << " " << f->first;
			first = false;
		};
		ret << " )\n";
	};
	return ret.str();
};

void CGuiSkin::ClearLayouts()
{
	Init();
	for( std::map< std::string, CGuiSkinnedLayout * > :: iterator it = m_instance->m_guis.begin();
		it != m_instance->m_guis.end(); it++ )
				delete it->second;
	m_instance->m_guis.clear();
};

static bool xmlGetBool(xmlNodePtr Node, const std::string& Name);
static int xmlGetInt(xmlNodePtr Node, const std::string& Name);
static float xmlGetFloat(xmlNodePtr Node, const std::string& Name);
static Uint32 xmlGetColor(xmlNodePtr Node, const std::string& Name);
static std::string xmlGetString(xmlNodePtr Node, const std::string& Name);
// Get the text inside element, like "<label rect="..."> Label text </label>"
static std::string xmlGetText(xmlDocPtr Doc, xmlNodePtr Node);


CGuiSkinnedLayout * CGuiSkin::GetLayout( const std::string & filename )
{
	Init();
	std::string skinpath( tLXOptions->sSkinPath );
	if( skinpath == "" )
		return NULL;
	skinpath += tLXOptions->sResolution;
	std::string filepath = "data/frontend/skins/" + skinpath + "/" + filename + ".xml";

	if( m_instance->m_guis.find( filepath ) != m_instance->m_guis.end() )
	{
		m_instance->m_guis[ filepath ]->ProcessGuiSkinEvent( CGuiSkin::SHOW_WIDGET );
		return m_instance->m_guis[ filepath ];
	};

	FILE * file = OpenGameFile( filepath, "r" );
	if( ! file )
	{
		printf("Cannot read GUI skin file %s\n", filepath.c_str() );
		return NULL;
	};

	std::string filedata;
	char buf[4096];
	while( ! feof(file) )
	{
		size_t read = fread( buf, 1, sizeof(buf), file );
		filedata.append(buf, read);
	};
	fclose(file);

	xmlDocPtr	Doc;
	xmlNodePtr	Node;
	CGuiSkinnedLayout * gui = new CGuiSkinnedLayout();

	Doc = xmlReadDoc( (const xmlChar *)filedata.c_str(), filepath.c_str(), NULL, XML_PARSE_NOBLANKS | XML_PARSE_NONET | XML_PARSE_NOCDATA );

	if (Doc == NULL)
	{
		printf("Cannot parse GUI skin file %s\n", filepath.c_str() );
		return NULL;
	};

	class InlineDataDeallocator{ public: xmlDocPtr m_doc;
			InlineDataDeallocator(xmlDocPtr doc): m_doc(doc) {};
			~InlineDataDeallocator() { xmlFreeDoc(m_doc); };
	} inlineDataDeallocator( Doc );

	Node = xmlDocGetRootElement(Doc);
	if (Node == NULL)
	{
		printf("GUI skin file %s is empty\n", filepath.c_str() );
		return NULL;
	};

	if ( stringcasecmp( (const char *)Node->name, "dialog" ) )
	{
		printf("GUI skin file %s is invalid: root item should be \"dialog\"\n", filepath.c_str() );
		return NULL;
	};

	for ( Node = Node->children; Node != NULL; Node = Node->next )
	{
		if ( Node->type != XML_ELEMENT_NODE )
			continue;

		int left   = xmlGetInt(Node,"left");
		int top    = xmlGetInt(Node,"top");
		int width  = xmlGetInt(Node,"width");
		int height = xmlGetInt(Node,"height");
		std::string s_id = xmlGetString(Node,"id");	// Widget ID (used for enable/disable it by func handlers)
		std::string init = xmlGetString(Node,"init");	// OnInit handler - fills list or combobox etc
		std::string s_pos = xmlGetString(Node,"rect");
		if( s_pos != "" )
		{
			std::vector<std::string> pos = explode( s_pos, "," ); // "left,top,width,height" as single string
			if( pos.size() > 0 )
				left = atoi( pos[0] );
			if( pos.size() > 1 )
				top = atoi( pos[1] );
			if( pos.size() > 2 )
				width = atoi( pos[2] );
			if( pos.size() > 3 )
				height = atoi( pos[3] );
		};
		std::map< std::string, std::pair< paramListVector_t, WidgetCreator_t > > :: iterator it;
		if( (!xmlStrcmp((const xmlChar *)Node->name,(const xmlChar *)"text")) ||
			(!xmlStrcmp((const xmlChar *)Node->name,(const xmlChar *)"comment")) )
		{	// Some extra newline or comment - skip it
			//printf("XML text inside \"%s\": \"%s\"\n", Node->parent->name, Node->content );
		}
		else
		for( it = m_instance->m_widgets.begin(); it != m_instance->m_widgets.end(); it++ )
		{
			if( stringcasecmp( it->first.c_str(), (const char *)Node->name ) )
				continue;

			std::vector< CScriptableVars::ScriptVar_t > params;
			for( paramListVector_t::const_iterator i = it->second.first.begin(); i != it->second.first.end(); i++ )
			{
				if( i->second == CScriptableVars::SVT_BOOL )
					params.push_back( CScriptableVars::ScriptVar_t( xmlGetBool( Node, i->first ) ) );
				else if( i->second == CScriptableVars::SVT_INT )
					params.push_back( CScriptableVars::ScriptVar_t( xmlGetInt( Node, i->first ) ) );
				else if( i->second == CScriptableVars::SVT_FLOAT )
					params.push_back( CScriptableVars::ScriptVar_t( xmlGetFloat( Node, i->first ) ) );
				else if( i->second == CScriptableVars::SVT_COLOR )
					params.push_back( CScriptableVars::ScriptVar_t( xmlGetColor( Node, i->first ) ) );
				else if( i->second == CScriptableVars::SVT_STRING )
				{	// "<label text="lalala"/>" and "<label>lalala</label>" are equal
					std::string text = xmlGetString( Node, i->first );
					if( text == "" && i->first == "text" )
						text = xmlGetText( Doc, Node );
					params.push_back( CScriptableVars::ScriptVar_t( text ) );
				}
				else params.push_back( CScriptableVars::ScriptVar_t( ) );	// Compile-time error here
			};

			int i_id = -1;
			if( s_id != "" )
				i_id = gui->GetIdByName( s_id );
			CWidget * widget = it->second.second( params, gui, i_id, left, top, width, height );
			if( init != "" )
			{
				CallbackHandler c_init( init, widget );
				c_init.Call();
			};
			break;
		};
		if( it == m_instance->m_widgets.end() )
		{
			printf("GUI skin file %s is invalid: invalid item \"%s\"\n", filepath.c_str(), Node->name );
		};
	};

	m_instance->m_guis[ filepath ] = gui;
	gui->ProcessGuiSkinEvent( CGuiSkin::SHOW_WIDGET );
	printf("GUI skin file %s loaded\n", filepath.c_str() );
	return gui;
};


///////////////////
// Get a bool from the specified property
bool xmlGetBool(xmlNodePtr Node, const std::string& Name)
{
	xmlChar *sValue = xmlGetProp(Node,(const xmlChar *)Name.c_str());
	if(!sValue)
		return false;
	bool result = false;
	if( !stringcasecmp( "true", (const char *)sValue ) )
		result = true;
	if( atoi((const char *)sValue) != 0 )
		result = true;
	xmlFree(sValue);
	return result;
}


///////////////////
// Get an integer from the specified property
int xmlGetInt(xmlNodePtr Node, const std::string& Name)
{
	xmlChar *sValue = xmlGetProp(Node,(const xmlChar *)Name.c_str());
	if(!sValue)
		return 0;
	int result = atoi((const char *)sValue);
	xmlFree(sValue);
	return result;
}

///////////////////
// Get a float from the specified property
float xmlGetFloat(xmlNodePtr Node, const std::string& Name)
{
	xmlChar *sValue = xmlGetProp(Node,(const xmlChar *)Name.c_str());
	if (!sValue)
		return 0;
	float result = (float)atof((const char *)sValue);
	xmlFree(sValue);
	return result;
}

///////////////////
// Get a colour from the specified property
Uint32 xmlGetColor(xmlNodePtr Node, const std::string& Name)
{
	xmlChar *sValue = xmlGetProp(Node,(const xmlChar *)Name.c_str());
	if (!sValue)
		return tLX->clPink;
	Uint32 result = StrToCol((char*)sValue).get();
	xmlFree(sValue);
	return result;
}

std::string xmlGetString(xmlNodePtr Node, const std::string& Name)
{
	xmlChar *sValue = xmlGetProp(Node,(const xmlChar *)Name.c_str());
	if (!sValue)
		return "";
	std::string ret = (const char *)sValue;
	xmlFree(sValue);
	return ret;
}

// Get the text inside element, like "<label rect="..."> Label text </label>"
std::string xmlGetText(xmlDocPtr Doc, xmlNodePtr Node)
{
	xmlChar *sValue = xmlNodeListGetString(Doc, Node->children, 1);
	if (!sValue)
		return "";
	std::string ret = (const char *)sValue;
	xmlFree(sValue);
	return ret;
};

void CGuiSkin::CallbackHandler::Init( const std::string & s1, CWidget * source )
{
	// TODO: put LUA handler here, this handmade string parser works but the code is ugly
	//printf( "CGuiSkin::CallbackHandler::Init(\"%s\"): ", s1.c_str() );
	m_source = source;
	m_callbacks.clear();
	std::string s(s1);
	TrimSpaces(s);
	while( s.size() > 0 )
	{
		std::string::size_type i = s.find_first_of( " \t(" );
		std::string func = s.substr( 0, i );
		std::string param;
		if( i != std::string::npos )
			i = s.find_first_not_of( " \t", i );
		if( i != std::string::npos )
			if( s[i] == '(' )	// Param
			{
				i++;
				std::string::size_type i1 = i;
				i = s.find_first_of( ")", i );
				if( i != std::string::npos )
				{
					param = s.substr( i1, i-i1 );
					i++;
				};
			};
		TrimSpaces(param);
		if( i != std::string::npos )
			s = s.substr( i );
		else
			s = "";
		TrimSpaces(s);

		std::map< std::string, CScriptableVars::ScriptVarPtr_t > :: iterator it;
		for( it = CScriptableVars::Vars().begin();
				it != CScriptableVars::Vars().end(); it++ )
		{
			if( !stringcasecmp( it->first, func ) && it->second.type == CScriptableVars::SVT_CALLBACK )
			{
				m_callbacks.push_back( std::pair< CScriptableVars::ScriptCallback_t, std::string > ( it->second.cb, param ) );
				//printf("%s(\"%s\") ", it->first.c_str(), param.c_str());
				break;
			};
		};
		if( it == CScriptableVars::Vars().end() )
		{
			printf("Cannot find GUI skin callback \"%s(%s)\"\n", func.c_str(), param.c_str());
		};
	};
	//printf("\n");
};

void CGuiSkin::CallbackHandler::Call()
{
	size_t size = m_callbacks.size();	// Some callbacks may destroy *this, m_callbacks.size() call will crash
	for( size_t f=0; f<size; f++ )	// I know that's hacky, oh well...
		m_callbacks[f].first( m_callbacks[f].second, m_source );	// Here *this may be destroyed
};

static bool bUpdateCallbackListChanged = false;

void CGuiSkin::RegisterUpdateCallback( CScriptableVars::ScriptCallback_t update, const std::string & param, CWidget * source )
{
	Init();
	m_instance->m_updateCallbacks.push_back( UpdateList_t( source, update, param ) );
	bUpdateCallbackListChanged = true;
};

void CGuiSkin::DeRegisterUpdateCallback( CWidget * source )
{
	Init();
	for( std::list< UpdateList_t > ::iterator it = m_instance->m_updateCallbacks.begin();
			it != m_instance->m_updateCallbacks.end(); )
	{
		if( it->source == source )
			m_instance->m_updateCallbacks.erase( it++ );	// Erase from std::list do not invalidate iterators
		else
			++it;
	};
	bUpdateCallbackListChanged = true;
};

void CGuiSkin::ProcessUpdateCallbacks()
{
	Init();
	for( std::list< UpdateList_t > ::iterator it = m_instance->m_updateCallbacks.begin();
			it != m_instance->m_updateCallbacks.end(); ++it )
	{
		std::string param = it->param;	// This string may be destroyed if DeRegisterUpdateCallback() called from update
		it->update( param, it->source );
		if( bUpdateCallbackListChanged )
		{
			bUpdateCallbackListChanged = false;
			return;	// Will update other callbacks next frame
		};
	};
};

// Old OLX menu system hooks
CGuiSkinnedLayout * MainLayout = NULL;
bool Menu_CGuiSkinInitialize(void)
{
	// TODO: don't hardcode window-size!
	DrawRectFill(tMenu->bmpBuffer.get(), 0, 0, 640-1, 480-1, tLX->clBlack);
	SetGameCursor(CURSOR_ARROW);
	tMenu->iMenuType = MNU_GUISKIN;
	MainLayout = CGuiSkin::GetLayout( "main" );
	if( MainLayout == NULL )
	{
		Menu_CGuiSkinShutdown();
		tLXOptions->sSkinPath = "";
		Menu_MainInitialize();
		return false;
	};

	return true;
};

void Menu_CGuiSkinFrame(void)
{
	if( ! MainLayout->Process() )
	{
		Menu_CGuiSkinShutdown();
		Menu_MainInitialize();
		return;
	};
	MainLayout->Draw(tMenu->bmpBuffer.get());
	DrawCursor(tMenu->bmpBuffer.get());
	DrawImage(VideoPostProcessor::videoSurface(), tMenu->bmpBuffer, 0, 0);	// TODO: hacky hacky, high CPU load
};

void Menu_CGuiSkinShutdown(void)
{
	DrawRectFill(tMenu->bmpBuffer.get(), 0, 0, 640-1, 480-1, tLX->clBlack);
	DrawImage(VideoPostProcessor::videoSurface(), tMenu->bmpBuffer, 0, 0);
	SetGameCursor(CURSOR_NONE);
	MainLayout = NULL;
	CGuiSkin::ClearLayouts();
};

// Some handy callbacks
void MakeSound( const std::string & param, CWidget * source )
{
	if( param == "" || param == "click" )
		PlaySoundSample(sfxGeneral.smpClick);
	if( param == "chat" )
		PlaySoundSample(sfxGeneral.smpChat);
	if( param == "ninja" )
		PlaySoundSample(sfxGame.smpNinja);
	if( param == "pickup" )
		PlaySoundSample(sfxGame.smpPickup);
	if( param == "bump" )
		PlaySoundSample(sfxGame.smpBump);
	if( param == "death" || param == "death1" )
		PlaySoundSample(sfxGame.smpDeath[0]);
	if( param == "death2" )
		PlaySoundSample(sfxGame.smpDeath[1]);
	if( param == "death3" )
		PlaySoundSample(sfxGame.smpDeath[2]);
};

	class GUISkinAdder
	{
		public:
	   	CCombobox* cb;
	   	int index;
		GUISkinAdder(CCombobox* cb_) : cb(cb_), index(1) {}
		inline bool operator() (std::string dir)
		{
			size_t slash = findLastPathSep(dir);
			if(slash != std::string::npos)
				dir.erase(0, slash+1);

			if( dir == ".svn" )
				return true;

			cb->addItem(index, dir, dir);

			index++;
			return true;
		};
	};

std::string sSkinCombobox_OldSkinPath("");	// We're in non-skinned menu by default

void SkinCombobox_Init( const std::string & param, CWidget * source )
{
	if( source->getType() != wid_Combobox )
		return;
	CCombobox * cb = ( CCombobox * ) (source);
	cb->setUnique(true);
	cb->clear();
	cb->addItem( 0, "", "None" );
	GUISkinAdder skinAdder(cb);
	FindFiles(skinAdder, "data/frontend/skins", false, FM_DIR);
	cb->setCurSIndexItem( tLXOptions->sSkinPath );
};

void SkinCombobox_Change( const std::string & param, CWidget * source )
{
	if( source->getType() != wid_Combobox )
		return;
	if( tLXOptions->sSkinPath != "" && sSkinCombobox_OldSkinPath == "" )	// Go to skinned menu from non-skinned menu
	{
		Menu_MainShutdown();
		Menu_CGuiSkinInitialize();
	}
	else	// Go from skinned menu to non-skinned menu or load another skin - handled in Menu_MainInitialize()
	{
		CGuiSkinnedLayout::ExitDialog( "", source );
	};
	sSkinCombobox_OldSkinPath = tLXOptions->sSkinPath;
};

void ExitApplication( const std::string & param, CWidget * source )
{
	if( Menu_MessageBox(GAMENAME,"Quit OpenLieroX?", LMB_YESNO) == MBR_YES )
	{
		tMenu->bMenuRunning = false;
		Menu_MainShutdown();
	};
};

void EnableWidget( const std::string & param, CWidget * source )
{
	CGuiSkinnedLayout * l = (CGuiSkinnedLayout *)source->getParent();
	if( !l )
		return;
	std::string trimmed(param);
	TrimSpaces(trimmed);
	CWidget * w = l->getWidget(trimmed);
	if( !w )
		return;
	w->setEnabled(true);
};

void DisableWidget( const std::string & param, CWidget * source )
{
	CGuiSkinnedLayout * l = (CGuiSkinnedLayout *)source->getParent();
	if( !l )
		return;
	std::string trimmed(param);
	TrimSpaces(trimmed);
	CWidget * w = l->getWidget(trimmed);
	if( !w )
		return;
	w->setEnabled(false);
};

std::string lx_version_string = LX_VERSION;

static bool bRegisteredCallbacks = CScriptableVars::RegisterVars("GUI")
	( & MakeSound, "MakeSound" )
	( & SkinCombobox_Init, "SkinCombobox_Init" )
	( & SkinCombobox_Change, "SkinCombobox_Change" )
	( & ExitApplication, "ExitApplication" )
	( lx_version_string, "ApplicationVersion" )
	( & EnableWidget, "EnableWidget" )
	( & DisableWidget, "DisableWidget" )
	;

}; // namespace DeprecatedGUI

// Register here some widgets that don't have their own .CPP files
// TODO: create different files for all of them

#include "DeprecatedGUI/CAnimation.h"

namespace DeprecatedGUI {
static bool CAnimation_WidgetRegistered =
	CGuiSkin::RegisterWidget( "animation", & CAnimation::WidgetCreator )
							( "file", CScriptableVars::SVT_STRING )
							( "frametime", CScriptableVars::SVT_FLOAT );
};

#include "DeprecatedGUI/CBox.h"

namespace DeprecatedGUI {
static bool CBox_WidgetRegistered =
	CGuiSkin::RegisterWidget( "box", & CBox::WidgetCreator )
							( "round", CScriptableVars::SVT_INT )
							( "border", CScriptableVars::SVT_INT )
							( "lightcolor", CScriptableVars::SVT_COLOR )
							( "darkcolor", CScriptableVars::SVT_COLOR )
							( "bgcolor", CScriptableVars::SVT_COLOR );
};

#include "DeprecatedGUI/CLabel.h"

namespace DeprecatedGUI {
static bool CLabel_WidgetRegistered =
	CGuiSkin::RegisterWidget( "label", & CLabel::WidgetCreator )
							( "text", CScriptableVars::SVT_STRING )
							( "color", CScriptableVars::SVT_COLOR )
							( "center", CScriptableVars::SVT_BOOL )
							( "var", CScriptableVars::SVT_STRING );
};

#include "DeprecatedGUI/CLine.h"

namespace DeprecatedGUI {
static bool CLine_WidgetRegistered =
	CGuiSkin::RegisterWidget( "line", & CLine::WidgetCreator )
							( "color", CScriptableVars::SVT_COLOR );
};

#include "DeprecatedGUI/CProgressbar.h"

namespace DeprecatedGUI {
static bool CProgressBar_WidgetRegistered =
	CGuiSkin::RegisterWidget( "progressbar", & CProgressBar::WidgetCreator )
							( "file", CScriptableVars::SVT_STRING )
							( "label_left", CScriptableVars::SVT_INT )
							( "label_top", CScriptableVars::SVT_INT )
							( "label_visible", CScriptableVars::SVT_BOOL )
							( "numstates", CScriptableVars::SVT_INT )
							( "var", CScriptableVars::SVT_STRING );
};
