/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Custom browser class
// Created 7/6/02
// Jason Boettcher


#ifndef __CWIDGETLIST_H__
#define __CWIDGETLIST_H__


// Widget list item structure
class widget_item_t { public:
	int				iID;
	UCString		sName;
	widget_item_t	*tNext;
};



// Widget list class
class CWidgetList {
public:
	CWidgetList() {
		tItems = NULL;
		iCount = 0;
	}

private:
	// Attributes
	widget_item_t	*tItems;
	int				iCount;

public:
	// Methods
	int		getCount(void)	{return iCount; }
	int		Add(const UCString& Name);
	UCString	getName(int ID);
	int		getID(const UCString& Name);
	void	Shutdown(void);
};

#endif  //  __CWIDGETLIST_H__
