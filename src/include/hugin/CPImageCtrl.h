// -*- c-basic-offset: 4 -*-
/** @file CPImageCtrl.h
 *
 *  @author Pablo d'Angelo <pablo.dangelo@web.de>
 *
 *  $Id$
 *
 *  This is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this software; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _CPIMAGECTRL_H
#define _CPIMAGECTRL_H

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include <vector>


/** Events to notify about new point / region / point change
 *
 */

class CPEvent : public wxCommandEvent
{
    DECLARE_DYNAMIC_CLASS(CPEvent)

    enum CPEventMode { NONE, NEW_POINT_CHANGED, POINT_SELECTED, POINT_CHANGED, REGION_SELECTED };
public:
    CPEvent( );
    /// a new point has been created.
    CPEvent(wxWindow* win, wxPoint & p);
    /// a point has been selected
    CPEvent(wxWindow *win, unsigned int cpNr);
    /// a point has been moved
    CPEvent(wxWindow* win, unsigned int cpNr, const wxPoint & p);
    /// region selected
    CPEvent(wxWindow* win, wxRect & reg);
    virtual wxEvent* Clone() const;

    /// accessor functions (they could check mode to see if a getXYZ() is
    /// allowed
    CPEventMode getMode()
        { return mode; };

    const wxRect & getRect()
        { return region; }

    const wxPoint & getPoint()
        { return point; }

    unsigned int getPointNr()
        { return pointNr; }
private:
    CPEventMode mode;
    wxRect region;
    wxPoint point;
    int pointNr;
};

typedef void (wxEvtHandler::*CPEventFunction)(CPEvent&);

BEGIN_DECLARE_EVENT_TYPES()
    DECLARE_EVENT_TYPE(EVT_CPEVENT,1)
END_DECLARE_EVENT_TYPES()

#define EVT_CPEVENT(func) \
    DECLARE_EVENT_TABLE_ENTRY( EVT_CPEVENT, \
                            -1,                       \
                            -1,                       \
                            (wxObjectEventFunction)   \
                            (CPEventFunction) & func, \
                            (wxObject *) NULL ),

// End of MyEvent.h


/** brief description.
 *
 *  What this does
 */
class CPImageCtrl : public wxScrolledWindow
{
public:

    /** ctor.
     */
    CPImageCtrl(wxWindow* parent, wxWindowID id = -1,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxHSCROLL | wxVSCROLL,
                const wxString& name="CPImageCtrl");


    CPImageCtrl()
        : drawNewPoint(false), scaleFactor(1),fitToWindow(false)
        { }

    /** dtor.
     */
    virtual ~CPImageCtrl();

    /// display img. every CPImageCtrl has a wxBitmap with
    /// its current image
    void setImage (const std::string & filename);

    /// control point inside this image
    void setCtrlPoints(const std::vector<wxPoint> & points);

    /// clear new point
    void clearNewPoint();

    /// select a point for usage
    void selectPoint(unsigned int);

    void mousePressEvent(wxMouseEvent *mouse);
    void mouseReleaseEvent(wxMouseEvent *mouse);
    void mouseMoveEvent(wxMouseEvent *mouse);

    wxSize DoGetBestSize() const;
//    virtual wxSize GetBestSize() const
//        { return DoGetBestSize(); }

    /** set the scaling factor for cp display.
     *
     *  @param factor zoom factor, 0 means fit to window.
     */
    void setScale(double factor);

    /** Show point @p x, @p y
     *
     *  Scrolls the windows so that x,y is shown in the center
     */
    void CPImageCtrl::showPosition(int x, int y);
    
    /** show the search area rectangle
     *
     */
    void showSearchArea(int width)
        { m_showSearchArea = true; m_searchRectWidth = width; }
    
    void hideSearchArea()
        { m_showSearchArea = false ; update(); }

protected:
    void drawPoint(wxDC & p, const wxPoint & point, const wxColor & color) const;
    void OnDraw(wxDC& dc);
    void OnSize(wxSizeEvent & e);

    /// helper func to emit a region
    bool emit(CPEvent & ev);

    /// get scale factor (calculates factor when fit to window is active)
    double getScaleFactor() const;

    /// calculate new scale factor for this image
    double calcAutoScaleFactor(wxSize size);

private:

    wxBitmap bitmap;
    std::string imageFilename;
    wxSize imageSize;

    std::vector<wxPoint> points;

    /// initiate redraw
    void update();

    int scale(int x) const
        {  return (int) (x * getScaleFactor() + 0.5); }
    wxPoint scale(const wxPoint & p) const
        {
            wxPoint r;
            r.x = scale(p.x);
            r.y = scale(p.y);
            return r;
        }

    int invScale(int x) const
        {  return (int) (x / getScaleFactor() + 0.5); }

    wxPoint invScale(const wxPoint & p) const
        {
            wxPoint r;
            r.x = invScale(p.x);
            r.y = invScale(p.y);
            return r;
        }

    // this is only valid during MOVE_POINT
    unsigned int selectedPointNr;
    // valid during MOVE_POINT and CREATE_POINT
    wxPoint point;
    bool drawNewPoint;
    wxPoint newPoint;

    // only valid during SELECT_REGION
    wxRect region;
    // state of widget (selection modes etc)
    // select region can also be used to just click...

    /**  state machine for selection process:
     *
     *   states:
     *    - NO_SELECTION nothing selected
     *        - KNOWN_POINT_SELECTED
     *          - mouse down on known point
     *          - set from outside
     *        - NEW_POINT_SELECTED
     *          - mouse down on new point (if one exists)
     *        - REGION
     *          - mouse down on unidentifed field
     *
     *    - KNOWN_POINT_SELECTED,
     *       a known point is selected and can be moved around.
     *       clients are notified about the movement, points can also
     *       be switched.
     *
     *        - NEW_POINT_SELECTED
     *          - mouse down on new point (if one exists)
     *        - REGION
     *          - mouse down on unidentifed field
     *
     *    - REGION the user can draw a bounding box.
     *        - NEW_POINT_SELECTED
     *          - mouse up on same point as mouse down.
     *        - NO_SELECTION
     *          - mouse up on different point, report selection
     *
     *    - NEW_POINT_SELECTED (can move new point), mouse up reports change,
     *           movement can be tracked
     *        - KNOWN_POINT_SELECTED
     *          - mouse down on known point
     *          - programatic change
     *        - REGION
     *          - mouse down on free space
     *
     */
    enum EditorState {NO_SELECTION, KNOWN_POINT_SELECTED, NEW_POINT_SELECTED, SELECT_REGION};
    EditorState editState;

    // colors for the different points
    std::vector<wxColour> pointColors;
    double scaleFactor;
    bool fitToWindow;

    bool m_showSearchArea;
    bool m_drawSearchRect;
    wxPoint m_mousePos;
    int m_searchRectWidth;

    /// check if p is over a known point, if it is, pointNr contains
    /// the point
    EditorState isOccupied(const wxPoint &p, unsigned int & pointNr) const;


    DECLARE_CLASS(CPImageCtrl)
    DECLARE_EVENT_TABLE()
};



#endif // _CPIMAGECTRL_H
