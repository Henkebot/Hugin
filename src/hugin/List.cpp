// -*- c-basic-offset: 4 -*-

/** @file List.cpp
 *
 *  @brief implementation of List Class
 *
 *  @author Kai-Uwe Behrmann <web@tiscali.de>
 *
 *  $Id$
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this software; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <wx/wxprec.h>
#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif

#include <wx/xrc/xmlres.h>          // XRC XML resouces

#include "PT/PanoCommand.h"
#include "hugin/config.h"
#include "hugin/CommandHistory.h"
#include "hugin/ImageCache.h"
#include "hugin/List.h"
#include "hugin/ImagesPanel.h"
#include "hugin/MainFrame.h"
#include "hugin/huginApp.h"
#include "PT/Panorama.h"

using namespace PT;

// image preview
extern wxBitmap * p_img;

// Image Icons
extern wxImageList * img_icons;
// Image Icons biger
extern wxImageList * img_bicons;

long int prevItem (-1);

// Image Preview
extern ImgPreview *canvas;

//------------------------------------------------------------------------------

BEGIN_EVENT_TABLE(List, wxListCtrl)
//    EVT_PAINT(ImagesPanel::OnDraw)
//    EVT_LIST_ITEM_SELECTED ( XRCID("images_list"), List::Change )
//    EVT_MOUSE_EVENTS ( List::Change )
    EVT_MOTION ( List::Change )
//    EVT_LEFT_DOWN ( List::Change )
END_EVENT_TABLE()


// Define a constructor for the Images Panel
List::List( wxWindow* parent, Panorama* pano, int layout)
    : wxListCtrl(parent, -1, wxDefaultPosition, wxDefaultSize, wxLC_REPORT|wxSUNKEN_BORDER),
      pano(*pano), list_layout (layout)
{
    DEBUG_TRACE("");

    InsertColumn( 0, _("#"), wxLIST_FORMAT_RIGHT, 25 );
    if ( list_layout == images_layout ) {
      InsertColumn( 1, _("Filename"), wxLIST_FORMAT_LEFT, 255 );
      InsertColumn( 2, _("width"), wxLIST_FORMAT_RIGHT, 60 );
      InsertColumn( 3, _("height"), wxLIST_FORMAT_RIGHT, 60 );
      InsertColumn( 4, _("No."), wxLIST_FORMAT_RIGHT, 30 );
      DEBUG_INFO( " images_layout" )
    } else {
      InsertColumn( 1, _("Filename"), wxLIST_FORMAT_LEFT, 180 );
      InsertColumn( 2, _("Lens type"), wxLIST_FORMAT_LEFT, 80 );
      InsertColumn( 3, _("Focal length"), wxLIST_FORMAT_RIGHT, 80 );
      InsertColumn( 4, _("a"), wxLIST_FORMAT_RIGHT, 20 );
      InsertColumn( 5, _("b"), wxLIST_FORMAT_RIGHT, 20 );
      InsertColumn( 6, _("c"), wxLIST_FORMAT_RIGHT, 20 );
      InsertColumn( 7, _("d"), wxLIST_FORMAT_RIGHT, 20 );
      InsertColumn( 8, _("e"), wxLIST_FORMAT_RIGHT, 20 );
      DEBUG_INFO( " else _layout" )
    }

    // Image Preview
    DEBUG_TRACE("");

    pano->addObserver(this);
    DEBUG_TRACE("");
}


List::~List(void)
{
    DEBUG_TRACE("");
    pano.removeObserver(this);
//    delete p_img; // Dont know how to check for existing
    p_img = (wxBitmap *) NULL;
    DEBUG_INFO( wxString::Format("modus %d", list_layout) )
}


//void List::panoramaChanged (PT::Panorama &pano) {}

void List::panoramaImagesChanged(Panorama &pano, const UIntSet &changed)       
{
    DEBUG_TRACE("");
    DeleteAllItems() ;
    DEBUG_INFO( wxString::Format("modus %d", list_layout) )

    // start the loop for every selected filename
    for ( int i = 0 ; i <= (int)pano.getNrOfImages() - 1 ; i++ ) {
        wxFileName fn = (wxString)pano.getImage(i).getFilename().c_str();
        wxImage * image = ImageCache::getInstance().getImage(
            pano.getImage(i).getFilename());

        // fill in the table
        char Nr[8] ;
        sprintf(Nr ,"%d", i + 1);
        InsertItem ( i, Nr, i );
        EnsureVisible(i);
        DEBUG_INFO( " icon at Item:" << wxString::Format("%d", i) << "/" << wxString::Format("%d", pano.getNrOfImages()) )
        SetItem ( i, 1, fn.GetFullName() );
        if ( list_layout == images_layout ) {
          sprintf(Nr, "%d", image->GetHeight() );
          SetItem ( i, 2, Nr );
          sprintf(Nr, "%d", image->GetWidth() );
          SetItem ( i, 3, Nr );
          sprintf(Nr, "%d", image->GetImageCount ( fn.GetFullPath() ) );
          SetItem ( i, 4, Nr );
        DEBUG_INFO( " images_layout" )
        } else {
          sprintf(Nr, "empty" );
          SetItem ( i, 2, Nr );
//          sprintf(Nr, "%d", pano.getImage(i)->Lens.projectionFormat );
          SetItem ( i, 3, Nr );
//          sprintf(Nr, "%f", pano.getImage(i)->Lens.HFOV );
          SetItem ( i, 4, Nr );
        DEBUG_INFO( " else _layout" )
        }
        SetColumnWidth(0, wxLIST_AUTOSIZE);
        SetColumnWidth(1, wxLIST_AUTOSIZE);
//        lst->SetItemImage( pano.getNrOfImages(), pano.getNrOfImages(), pano.getNrOfImages() );

    }
/*
    if ( pano.getNrOfImages() == 0 ) {
        delete p_img;
        p_img = new wxBitmap(0,0);
    }*/
    DEBUG_TRACE("");
}

/*void List::Change ( wxListEvent & e )
{
    DEBUG_TRACE ("")
    long item (1);
//    wxPoint pos = e.GetPosition();
//    long item = HitTest( e.m_x ,e.m_y );
//    DEBUG_INFO ( "hier:" << wxString::Format(" %d is item %ld", e.GetPosition(), item) );
    DEBUG_INFO ( "hier: is item %ld" << wxString::Format("%ld", item) );
    frame->SetStatusBar(wxString::Format(" %d,%d is item %ld", e.m_x, e.m_y, item), 1);
}*/

void List::Change ( wxMouseEvent & e )
{
//    DEBUG_TRACE ("")
    int flags = wxLIST_HITTEST_ONITEM;
    long item = HitTest( e.GetPosition(),  flags);
//    frame->SetStatusText(wxString::Format(" %d,%d / %ld",e.m_x,e.m_y, item), 1);
    
    if ( item != -1 && item != prevItem ) {
        // preview selected images
        wxImage * s_img = ImageCache::getInstance().getImageSmall(
            pano.getImage((int)item).getFilename());


        // right image preview
        wxImage r_img;
        if ( s_img->GetHeight() > s_img->GetWidth() ) {
          r_img = s_img->Scale( (int)((float)s_img->GetWidth()/
                                      (float)s_img->GetHeight()*128.0),
                                128);
        } else {
          r_img = s_img->Scale( 128,
                                (int)((float)s_img->GetHeight()/
                                      (float)s_img->GetWidth()*128.0));
        }
        delete p_img;
        p_img = new wxBitmap( r_img.ConvertToBitmap() );
        canvas->Refresh();
        frame->SetStatusText(wxString::Format(" %d,%d / %ld",e.m_x,e.m_y, item), 1);
    }
    if (item != -1)
      prevItem = item;
}

