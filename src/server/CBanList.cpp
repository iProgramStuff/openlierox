/////////////////////////////////////////
//
//             Liero Xtreme
//
//        Copyright RIP Clan 2006
//
//
/////////////////////////////////////////


// Ban List class
// Created 9/8/06
// Dark Charlie


#include "defs.h"
#include "LieroX.h"


///////////////////
// BanList Constructor
CBanList::CBanList()
{
    m_psBanList = NULL;
    m_psSortedList = NULL;
    m_nCount = 0;
	m_szPath = "cfg/ban.lst";
	m_bLoading = false;
}

///////////////////
// Find a banned worm in the list
banlist_t *CBanList::findBanned(char *szAddress)
{
	if (m_nCount <= 0)
		return NULL;

    assert( szAddress );

    static char address[256];

	// Remove the port from the address
	char *addr;
	addr = szAddress;
	char *p = strrchr(addr,':');
	if(p) {
		int pos = p - addr + 1;
		addr[pos-1] = '\0';
	}

    lx_strncpy(address, addr, 16);

    banlist_t *psWorm = m_psBanList;

    for(; psWorm; psWorm=psWorm->psNext) {
        fix_strncpy(address, TrimSpaces( address ));
        if( stricmp(psWorm->szAddress, address) == 0 )
            return psWorm;
    }

    // No match
    return NULL;
}

///////////////////
// Get the item ID from an address
int CBanList::getIdByAddr(char *szAddress)
{
	if (m_nCount <= 0)
		return -1;

    assert( szAddress );

    static char address[256];

	// Remove the port from the address
	char *addr;
	addr = szAddress;
	char *p = strrchr(addr,':');
	if(p) {
		int pos = p - addr + 1;
		addr[pos-1] = '\0';
	}

    lx_strncpy(address, addr, 16);

    banlist_t *psWorm = m_psBanList;

    for(int i=0; psWorm; psWorm=psWorm->psNext,i++) {
        fix_strncpy(address, TrimSpaces( address ));
        if( stricmp(psWorm->szAddress, address) == 0 )
            return i;
    }

    // No match
    return -1;
}


///////////////////
// Ban a worm
void CBanList::addBanned(char *szAddress, char *szNick)
{
    if (!szNick)
		szNick = " ";
	assert( szAddress );

	// Remove the port from the address
	char *addr;
	addr = szAddress;
	char *p = strrchr(addr,':');
	if(p) {
		int pos = p - addr + 1;
		addr[pos-1] = '\0';
	}

    banlist_t *psWorm = new banlist_t;

    if( !psWorm )
        return;

    size_t len = strlen(szNick);
    psWorm->szNick = new char[ len+1 ];
    memcpy(psWorm->szNick, szNick, len+1);
    
    len = strlen(addr);
    psWorm->szAddress = new char[ len+1 ];
    memcpy(psWorm->szAddress, addr, len+1);
    
    // Link it in
    psWorm->psNext = m_psBanList;
    m_psBanList = psWorm;

    // Sort the list
    sortList();

	if (!m_bLoading)
		saveList(m_szPath);
}

///////////////////
// Unban a worm
void CBanList::removeBanned(char *szAddress)
{
	if (!szAddress)
		return;

	assert( szAddress );

	// Remove the port from the address
	char *addr;
	addr = szAddress;
	char *p = strrchr(addr,':');
	if(p) {
		int pos = p - addr + 1;
		addr[pos-1] = '\0';
	}

    banlist_t *psWorm = findBanned(szAddress);
	if (!psWorm)
		return;
	int ID = getIdByAddr(szAddress);
	if (ID == -1)
		return;

	// Previous worm in ban list
	banlist_t *psPrevWorm = getItemById(ID-1);
	if (!psPrevWorm)  { // our worm is the first in the list
		if (psWorm->psNext) 
		  m_psBanList = psWorm->psNext;  // update the pointer to the first item
		else
		  m_psBanList = NULL;  // the worm is (was) the only one in the list
		
		// Unban the worm
		delete[] psWorm->szNick;
		delete[] psWorm->szAddress;
		delete psWorm;
		psWorm = NULL;
	}
	else  {  // our worm isn't the first in the list
		if (psWorm->psNext)
			psPrevWorm->psNext = psWorm->psNext;  // our worm is somewhere in the middle of the list
		else
			psPrevWorm->psNext = NULL;  // our worm is the last in the list
		
		// Unban the worm
		delete[] psWorm->szNick;
		delete[] psWorm->szAddress;
		delete psWorm;
		psWorm = NULL;
	}

	m_nCount--;  // update the number of items

    // Sort the list
    sortList();

	// Save the list
	saveList(m_szPath);
}


///////////////////
// Save the ban list
void CBanList::saveList(char *szFilename)
{
    // Save it as plain text
    FILE *fp = OpenGameFile(szFilename, "wt");
    if( !fp )
        return;

    banlist_t *psWorm = m_psBanList;
	if (!psWorm)
		fputs("",fp);
	else {
		for(; psWorm; psWorm=psWorm->psNext) {

			fprintf(fp, "%s,%s\n", psWorm->szAddress, psWorm->szNick);
		}
	}  // else

    fclose(fp);
}


///////////////////
// Load the ban list
void CBanList::loadList(char *szFilename)
{
	m_bLoading = true;
    // Shutdown the list first
    Shutdown();

    FILE *fp = OpenGameFile(szFilename, "rt");
    if( !fp )
        return;

    static char line[256];
    strcpy(line, "");

    while( !feof(fp) ) {
        fscanf(fp, "%[^\n]\n",line);

		if (!strchr(line,','))
			continue;

        char *tok = strtok(line,",");
        if( tok ) 
            addBanned( tok, strtok(NULL,",") );
    }

    fclose(fp);

    // Sort the list
    sortList();

	m_bLoading = false;
}

///////////////////
// Clears the list
void CBanList::Clear(void)
{
	Shutdown();
}

///////////////////
// Checks if the IP is banned
bool CBanList::isBanned(char *szAddress)
{
	if (strstr(szAddress,"127.0.0.1"))
		return false;

    banlist_t *psWorm = findBanned(szAddress);

    // If we can't find the worm, it's not banned
    if( !psWorm )
        return false;
    
    return true;
}



///////////////////
// Return the sorted ban list
banlist_t *CBanList::getList(void)
{
	sortList();
    return m_psSortedList;
}


///////////////////
// Return the number of banned IPs
int CBanList::getNumItems(void)
{
    return m_nCount;
}


///////////////////
// Create a sorted list
void CBanList::sortList(void)
{
    int i, j;

    // Free any previous list
    if( m_psSortedList )
        delete[] m_psSortedList;

    // Count the number of banned worms
    m_nCount = 0;
    banlist_t *psWorm = m_psBanList;
    for(; psWorm; psWorm=psWorm->psNext)  {
        m_nCount++;
	}

    // Allocate the sorted list
    m_psSortedList = new banlist_t[m_nCount];
    if( !m_psSortedList )
        return;

    // Fill in the links
    psWorm = m_psBanList;
    for( i=0; i<m_nCount; i++, psWorm=psWorm->psNext)
        m_psSortedList[i].psLink = psWorm;
    
    if( m_nCount < 2 )
        return;

    // Sort the list using a simple bubble sort
    banlist_t temp;
   	for(i=0; i<m_nCount; i++) {
		for(j=0; j<m_nCount-1-i; j++) {

            if( strcmp(m_psSortedList[j].psLink->szNick, m_psSortedList[j+1].psLink->szNick) > 0 ) {

                // Swap the 2 items
                temp = m_psSortedList[j];
                m_psSortedList[j] = m_psSortedList[j+1];
                m_psSortedList[j+1] = temp;
            }
        }
    }
}

///////////////////
// Path to the ban list file
char *CBanList::getPath(void)
{
	return m_szPath;
}

///////////////////
// Get the specified item
banlist_t *CBanList::getItemById(int ID)
{
    if (ID > m_nCount || ID < 0)
		return NULL;
	banlist_t *psWorm = m_psBanList;

	if(!psWorm)
		return NULL;

    for(int i=0; psWorm; psWorm=psWorm->psNext,i++)  {
		if (ID == i) 
		  return psWorm;
	}

	return NULL;
	
}

///////////////////
// Shutdown the ban list
void CBanList::Shutdown(void)
{
     banlist_t *psWorm = m_psBanList;
     banlist_t *psNext = NULL;

     for(; psWorm; psWorm=psNext) {
         psNext = psWorm->psNext;

         if( psWorm->szNick )
             delete[] psWorm->szNick;
		 if( psWorm->szAddress )
             delete[] psWorm->szAddress;

         delete psWorm;
     }

     m_psBanList = NULL;

    // Free any sorted list
    if( m_psSortedList )
        delete[] m_psSortedList;
    m_psSortedList = NULL;
}
