// -*- c-basic-offset: 4 -*-

/** @file Panorama.cpp
 *
 *  @brief implementation of Panorama Class
 *
 *  @author Pablo d'Angelo <pablo.dangelo@web.de>
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

#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <set>
#include <iterator>
#include <algorithm>

#include <stdio.h>
#include <math.h>

#include "PT/Panorama.h"
#include "PT/Process.h"
#include "common/utils.h"
#include "common/stl_utils.h"

using namespace PT;
using namespace std;


/// helper functions for parsing of a script line
bool PT::getPTParam(std::string & output, const std::string & line, const std::string & parameter)
{
    std::string::size_type p;
    if ((p=line.find(std::string(" ") + parameter)) == std::string::npos) {
//        DEBUG_ERROR("could not find param " << parameter
//                    << " in line: " << line);
        return false;
    }
    p += 2;
    std::string::size_type p2 = line.find(' ',p);
    output = line.substr(p, p2-p);
//    DEBUG_DEBUG("string idex: " << p <<"," << p2 << "  string: \"" << output << "\"");
    return true;
}

bool PT::getPTStringParam(std::string & output, const std::string & line, const std::string & parameter)
{
    std::string::size_type p;
    if ((p=line.find(std::string(" ") + parameter + "\"")) == std::string::npos) {
        DEBUG_ERROR("could not find string param " << parameter
                    << " in line: " << line);
        return false;
    }
    p += 3;
    std::string::size_type e = line.find("\"",p);
    DEBUG_DEBUG("p:" << p << " e:" << e);
    output = line.substr(p,e-p);
    DEBUG_DEBUG("output: ##" << output << "##");
    return true;
}

bool PT::readVar(Variable & var, int & link, const std::string & line)
{
    string val;
    if (getPTParam(val,line, var.getName())) {
        DEBUG_ASSERT(line.size() > 0);
        DEBUG_DEBUG(var.getName() << ":" <<val);
        if (val[0] == '=') {
            link = utils::lexical_cast<int>(val.substr(1));
        } else {
            link = -1;
            var.setValue(utils::lexical_cast<double>(val));
        }
    } else {
        return false;
    }
    return true;
}



//=========================================================================
//=========================================================================


Panorama::Panorama()
    : currentProcess(NO_PROCESS),
      optimizerExe("PTOptimizer"),
      stitcherExe("PTStitcher"),
      PTScriptFile("PT_script.txt")
{
    cerr << "Panorama obj created" << endl;
/*
    settings.setPath("dangelo","PanoAssistant");
    readSettings();
    process.setCommunication(QProcess::Stdin|
                             QProcess::Stdout|
                             QProcess::Stderr|
                             QProcess::DupStderr);
    connect(&process, SIGNAL(processExited()), this, SLOT(processExited()));
*/
}


Panorama::~Panorama()
{
    reset();
    changeFinished();
}


void Panorama::reset()
{
    // delete all images and control points.
    state.ctrlPoints.clear();
    state.lenses.clear();
    state.images.clear();
    state.variables.clear();
}


#if 0
QDomElement Panorama::toXML(QDomDocument & doc)
{
    QDomElement root = doc.createElement("panorama");
    // serialize global options:
    root.appendChild(options.toXML(doc));

    // serialize image
    QDomElement images_ = doc.createElement("images");
    for (ImageVector::iterator it = images.begin(); it != images.end(); ++it) {
        images_.appendChild(it->toXML(doc));
    }
    root.appendChild(images_);

    // control points
    QDomElement cps = doc.createElement("control_points");
    for (CPVector::iterator it = state.ctrlPoints.begin(); it != state.ctrlPoints.end(); ++it) {
        cps.appendChild(it->toXML(doc));
    }
    root.appendChild(cps);

    // lenses
    QDomElement lenses_ = doc.createElement("lenses");
    for (std::vector<Lens>::iterator it = lenses.begin(); it != lenses.end(); ++it) {
        lenses_.appendChild(it->toXML(doc));
    }
    root.appendChild(lenses_);

    return root;
}

void Panorama::setFromXML(const QDomNode & elem)
{
    DEBUG_DEBUG("Panorama::setFromXML");
    clear();

    Q_ASSERT(elem.nodeName() == "panorama");
    // read global options
    QDomNode n = elem.namedItem("output");
    Q_ASSERT(!n.isNull);
    options.setFromXML(n);
    n = elem.namedItem("images");
    Q_ASSERT(!n.isNull);
    n = n.firstChild();
    while( !n.isNull() ) {
        QDomElement e = n.toElement(); // try to convert the node to an element.
        Q_ASSERT((!e.isNull()) && e.tagName() == "image" );
        images.push_back(PanoImage(*this, e));
        reportAddedImage(images.size() -1);
        n = n.nextSibling();
    }


    n = elem.namedItem("control_points");
    Q_ASSERT(!n.isNull());
    n = n.firstChild();
    while( !n.isNull() ) {
        QDomElement e = n.toElement();// try to convert the node to an element.
        Q_ASSERT((!e.isNull()) && e.tagName() == "control_point" );
        controlPoints.push_back(ControlPoint(*this, e));
        reportAddedCtrlPoint(controlPoints.size() -1);
        n = n.nextSibling();
    }
}


#endif


std::vector<unsigned int> Panorama::getCtrlPointsForImage(unsigned int imgNr) const
{
    std::vector<unsigned int> result;
    unsigned int i = 0;
    for (CPVector::const_iterator it = state.ctrlPoints.begin(); it != state.ctrlPoints.end(); ++it) {
        std::cout << "c n" << it->image1Nr
          << " N" << it->image2Nr
          << " x" << it->x1 << " y" << it->y1
          << " X" << it->x2 << " Y" << it->y2
          << " t" << it->mode << std::endl;
        if ((it->image1Nr == imgNr) || (it->image2Nr == imgNr)) {
            result.push_back(i);
        }
        i++;
    }
    return result;
}

const VariableMapVector & Panorama::getVariables() const
{
    return state.variables;
}

const VariableMap & Panorama::getImageVariables(unsigned int imgNr) const
{
    assert(imgNr < state.images.size());
    return state.variables[imgNr];
}


double Panorama::calcHFOV() const
{
    double hfov = 0;
    int nImages = state.images.size();
    for (int i = 0; i <nImages; i++) {
        double c = fabs(map_get(state.variables[i],"y").getValue());
        c += map_get(state.variables[i],"v").getValue() / 2;
        if (c > hfov) {
            hfov = c;
        }
    }
    return 2 * hfov;
}

double Panorama::calcVFOV() const
{
    double vfov = 0;
    int nImages = state.images.size();
    for (int i = 0; i <nImages; i++) {
        double c = fabs(map_get(state.variables[i],"p").getValue());
        double w = state.images[i].getWidth();
        double h = state.images[i].getHeight();
        c += map_get(state.variables[i],"v").getValue() * h/w / 2;
        if (c > vfov) {
            vfov = c;
        }
    }
    return 2 * vfov;
}

void Panorama::updateCtrlPointErrors(const CPVector & cps)
{
    assert(cps.size() == state.ctrlPoints.size());
    unsigned int nrp = cps.size();
    for (unsigned int i = 0; i < nrp ; i++) {
        imageChanged(state.ctrlPoints[i].image1Nr);
        imageChanged(state.ctrlPoints[i].image2Nr);
        state.ctrlPoints[i].error = cps[i].error;
    }
}

void Panorama::updateVariables(const VariableMapVector & vars)
{
    assert(vars.size() == state.images.size());
    unsigned int i = 0;
    for (VariableMapVector::const_iterator it = vars.begin(); it != vars.end(); ++it) {
        updateVariables(i, *it);
        i++;
    }
}

void Panorama::updateVariables(unsigned int imgNr, const VariableMap & var)
{
    assert(imgNr < state.images.size());
    for (VariableMap::const_iterator it = var.begin(); it != var.end() ; ++it) {
        updateVariable(imgNr,it->second);
    }
}

void Panorama::updateVariable(unsigned int imgNr, const Variable &var)
{
    DEBUG_TRACE("image " << imgNr << " variable: " << var.getName());
    DEBUG_ASSERT(imgNr < state.images.size());
    // update a single variable
    // check corrosponding lens if we have to update some other images
    // as well.
    unsigned int lensNr = state.images[imgNr].getLensNr();
    DEBUG_ASSERT(lensNr < state.lenses.size());

    // update value for this image
//    state.variables[imgNr][var.getName()].setValue(var.getValue());
    map_get(state.variables[imgNr],var.getName()).setValue(var.getValue());
    bool lensVar = set_contains(state.lenses[lensNr].variables, var.getName());
    if (lensVar) {
        // special handling for lens variables.
        // if they are inherited, update the value in the lens, and all
        // image variables that use this lens.
        LensVariable & lv = map_get(state.lenses[lensNr].variables,var.getName());
        if (lv.isLinked()) {
            DEBUG_DEBUG("updating image variable, lens var is linked");
            lv.setValue(var.getValue());
            updateLensVariable(lensNr,lv);
        }
    }
    imageChanged(imgNr);
}

unsigned int Panorama::addImage(const PanoImage &img, const VariableMap & vars)
{
    // the lens must have been already created!
    DEBUG_ASSERT(img.getOptions().lensNr < state.lenses.size());
    unsigned int nr = state.images.size();
    state.images.push_back(img);
    state.variables.push_back(vars);
    copyLensVariablesToImage(nr);
    imageChanged(nr);
    return nr;
}

#if 0
unsigned int Panorama::addImage(const std::string & filename)
{
    // create a lens if we don't have one.
    if (state.lenses.size() < 1) {
        state.lenses.push_back(Lens());
    }


    // read lens spec from image, if possible
    // FIXME use a lens database (for example the one from PTLens)
    // FIXME to initialize a,b,c etc.


    // searches for the new image for an unused lens , if found takes this
    // if no free lens is available creates a new one
    int unsigned lensNr (0);
    bool lens_belongs_to_image = false;
    int unused_lens = -1;
    while ( unused_lens < 0 ) {
      lens_belongs_to_image = false;
      for (ImageVector::iterator it = state.images.begin();
                                    it != state.images.end()  ; ++it) {
          if ((*it).getLens() == lensNr)
              lens_belongs_to_image = true;
      }
      if ( lens_belongs_to_image == false )
        unused_lens = lensNr;
      else
        lensNr++;
    }
    bool lens_allready_inside = false;
    for ( lensNr = 0 ; lensNr < state.lenses.size(); ++lensNr) {
        if ( (int)lensNr == unused_lens )
          lens_allready_inside = true;
    }
//    DEBUG_INFO ( "lens_allready_inside= "<< lens_allready_inside <<"  new lensNr: " << unused_lens <<"/"<< state.lenses.size() )
    Lens l;
    if ( lens_allready_inside )
        l = state.lenses[unused_lens];
    l.readEXIF(filename);
    if ( lens_allready_inside ) {
        state.lenses[unused_lens] = l;
    } else {
        state.lenses.push_back(l);
        unused_lens = state.lenses.size() - 1;
    }

    unsigned int nr = state.images.size();
    state.images.push_back(PanoImage(filename));
    ImageOptions opts = state.images.back().getOptions();
    opts.lensNr = unused_lens;
    state.images.back().setOptions(opts);
    state.variables.push_back(ImageVariables());
    updateLens(nr);
    adjustVarLinks();
    imageChanged(nr);
    DEBUG_INFO ( "new lensNr: " << unused_lens <<"/"<< state.lenses.size() )
    return nr;
}

#endif

void Panorama::removeImage(unsigned int imgNr)
{
    DEBUG_DEBUG("Panorama::removeImage(" << imgNr << ")");
    assert(imgNr < state.images.size());

    // remove control points
    CPVector::iterator it = state.ctrlPoints.begin();
    while (it != state.ctrlPoints.end()) {
        if ((it->image1Nr == imgNr) || (it->image2Nr == imgNr)) {
            // remove point that refernce to imgNr
            it = state.ctrlPoints.erase(it);
        } else {
            // correct point references
            if (it->image1Nr > imgNr) it->image1Nr--;
            if (it->image2Nr > imgNr) it->image2Nr--;
            ++it;
        }
    }

    // remove Lens if needed
    bool removeLens = true;
    unsigned int lens = state.images[imgNr].getLensNr();
    unsigned int i = 0;
    for (ImageVector::iterator it = state.images.begin(); it != state.images.end(); ++it) {
        if ((*it).getLensNr() == lens && imgNr != i) {
            removeLens = false;
        }
        i++;
    }
    if (removeLens) {
        for (ImageVector::iterator it = state.images.begin(); it != state.images.end(); ++it) {
            if((*it).getLensNr() >= lens) {
                (*it).setLensNr((*it).getLensNr() - 1);
                imageChanged(it - state.images.begin());
            }
        }
        state.lenses.erase(state.lenses.begin() + lens);
    }

    state.variables.erase(state.variables.begin() + imgNr);
    state.images.erase(state.images.begin() + imgNr);

    // change all other (moved) images
    for (unsigned int i=imgNr; i < state.images.size(); i++) {
        imageChanged(i);
    }
}


unsigned int Panorama::addCtrlPoint(const ControlPoint & point )
{
    unsigned int nr = state.ctrlPoints.size();
    state.ctrlPoints.push_back(point);
    imageChanged(point.image1Nr);
    imageChanged(point.image2Nr);
    return nr;
}

void Panorama::removeCtrlPoint(unsigned int pNr)
{
    assert(pNr < state.ctrlPoints.size());
    ControlPoint & point = state.ctrlPoints[pNr];
    unsigned int i1 = point.image1Nr;
    unsigned int i2 = point.image2Nr;
    state.ctrlPoints.erase(state.ctrlPoints.begin() + pNr);
    imageChanged(i1);
    imageChanged(i2);
}


void Panorama::changeControlPoint(unsigned int pNr, const ControlPoint & point)
{
    assert(pNr < state.ctrlPoints.size());

    // change notify for all involved images
    imageChanged(state.ctrlPoints[pNr].image1Nr);
    imageChanged(state.ctrlPoints[pNr].image2Nr);
    imageChanged(point.image1Nr);
    imageChanged(point.image2Nr);

    state.ctrlPoints[pNr] = point;
}

void Panorama::printOptimizerScript(ostream & o,
                                    const OptimizeVector & optvars,
                                    const PanoramaOptions & output)
{
    o << "# PTOptimizer script, written by hugin" << endl
      << endl;
    // output options..

    output.printScriptLine(o);

    unsigned int i = 0;
    std::map<unsigned int, unsigned int> linkAnchors;
    o << endl
      << "# image lines" << endl;
    for (ImageVector::const_iterator it = state.images.begin(); it != state.images.end(); ++it) {
        o << "i w" << (*it).getWidth() << " h" << (*it).getHeight()
          <<" f" << state.lenses[(*it).getLensNr()].projectionFormat << " ";
        // print variables with links

        unsigned int imgNr = it - state.images.begin();
        unsigned int lensNr = it->getLensNr();
        Lens & lens = state.lenses[it->getLensNr()];
        const VariableMap & vars = state.variables[imgNr];
        for (VariableMap::const_iterator vit = vars.begin();
             vit != vars.end(); ++vit)
        {
            // print links if needed
            if (set_contains(lens.variables,vit->first)
                && map_get(lens.variables, vit->first).isLinked())
            {
                if (set_contains(linkAnchors, lensNr)
                    && linkAnchors[lensNr] != imgNr)
                {
                    // print link
                    DEBUG_DEBUG("printing link: " << vit->first);
                    // print link, anchor variable was already printed
                    map_get(lens.variables,vit->first).printLink(o,linkAnchors[lensNr]) << " ";
                } else {
                    DEBUG_DEBUG("printing value for linked var " << vit->first);
                    // first time, print value
                    linkAnchors[lensNr] = imgNr;
                    vit->second.print(o) << " ";
                }
            } else {
                // simple variable, just print
                vit->second.print(o) << " ";
            }
        }

        o << " u" << (*it).getOptions().featherWidth
          << ((*it).getOptions().morph ? " o" : "")
          << " n\"" << (*it).getFilename() << "\"" << std::endl;
        i++;
    }

    o << endl << endl
      << "# specify variables that should be optimized" << endl
      << "v ";

    // be careful. linked variables should not be specified multiple times.
    vector<set<string> > linkvars(state.lenses.size());
    for (unsigned int i=0; i < state.images.size(); i++) {

        unsigned int lensNr = state.images[i].getLensNr();
        const Lens & lens = state.lenses[lensNr];
        const set<string> & optvar = optvars[i];
        for (set<string>::iterator sit = optvar.begin();
             sit != optvar.end(); ++sit )
        {
            if (set_contains(lens.variables,*sit)) {
                // it is a lens variable
                if (map_get(lens.variables,*sit).isLinked() &&
                    ! set_contains(linkvars[lensNr], *sit))
                {
                    // print only once
                    o << *sit << i << " ";
                    linkvars[lensNr].insert(*sit);
                }
            } else {
                // not a lens variable, print multiple times
                o << *sit << i << " ";
            }
        }
    }
    o << endl << endl
      << "# control points" << endl;
    for (CPVector::const_iterator it = state.ctrlPoints.begin(); it != state.ctrlPoints.end(); ++it) {
        o << "c n" << it->image1Nr
          << " N" << it->image2Nr
          << " x" << it->x1 << " y" << it->y1
          << " X" << it->x2 << " Y" << it->y2
          << " t" << it->mode << std::endl;
    }
    o << endl;
}


void Panorama::printStitcherScript(ostream & o,
                                   const PanoramaOptions & target)
{
    o << "# PTStitcher script, written by hugin" << endl
      << endl;
    // output options..
    target.printScriptLine(o);
    o << endl
      << "# output image lines" << endl;
    unsigned int i=0;
    for (ImageVector::const_iterator it = state.images.begin(); it != state.images.end(); ++it) {

        o << "o w" << (*it).getWidth() << " h" << (*it).getHeight()
          <<" f" << state.lenses[(*it).getLensNr()].projectionFormat << " ";
        // print variables, without links
        VariableMap::const_iterator vit;
        for(vit = state.variables[i].begin();
            vit != state.variables[i].end();  ++vit)
        {
            vit->second.print(o) << " ";
        }
        o << " u" << (*it).getOptions().featherWidth << " m" << (*it).getOptions().ignoreFrameWidth
          << ((*it).getOptions().morph ? " o" : "")
          << " n\"" << (*it).getFilename() << "\"" << std::endl;
        i++;
    }
    o << endl;
}

void Panorama::readOptimizerOutput(VariableMapVector & vars, CPVector & ctrlPoints) const
{
    std::ifstream script(PTScriptFile.c_str());
    if (!script.good()) {
        DEBUG_ERROR("Could not open " << PTScriptFile);
        // throw execption
        return;
    }
    parseOptimizerScript(script, vars, ctrlPoints);
}

void Panorama::parseOptimizerScript(istream & i, VariableMapVector & imgVars, CPVector & CPs) const
{
    DEBUG_TRACE("");
    // 0 = read output (image lines), 1 = read control point distances
    int state = 0;
    string line;
    unsigned int lineNr = 0;
    VariableMapVector::iterator varIt = imgVars.begin();
    CPVector::iterator pointIt = CPs.begin();

    int pnr=0;

    while (!i.eof()) {
        std::getline(i, line);
        lineNr++;
        switch (state) {
        case 0:
        {
            // we are reading the output lines:
            // o f3 r0 p0 y0 v89.2582 a-0.027803 b0.059851 c-0.073115 d10.542470 e16.121145 u10 -buf
            if ((line.compare("# Control Points: Distance between desired and fitted Position") == 0 )
             || (line.compare("# Control Points: Distance between desired and fitted Position (in Pixels)")) == 0 ) {
                // switch to reading the control point distance
                assert(varIt == imgVars.end());
                state = 1;
                break;
            }
            if (line[0] != 'o') continue;
            assert(varIt != imgVars.end());
            DEBUG_DEBUG("reading image variables");
            // read position variables
            int link;
            readVar(map_get(*varIt, "r"), link, line);
            DEBUG_ASSERT(link == -1);
            readVar(map_get(*varIt, "p"), link, line);
            DEBUG_ASSERT(link == -1);
            readVar(map_get(*varIt, "y"), link, line);
            DEBUG_ASSERT(link == -1);

            DEBUG_DEBUG("yaw: " << map_get(*varIt, "y").getValue()
                        << " pitch " << map_get(*varIt, "p").getValue()
                        << " roll " << map_get(*varIt, "r").getValue());
            // read lens variables
            char *varchars[] = { "v","a","b","c","d","e", NULL };
            for (char **c = varchars; *c != 0; ++c) {
                Variable & curVar = map_get(*varIt, *c);
                if (!readVar(curVar, link, line)) {
                    DEBUG_ERROR("Could not read "<< *c << " at script line " << lineNr);
                }
                // linking in output forbidden
                DEBUG_ASSERT(link == -1);
            }
            varIt++;
            break;
        }
        case 1:
        {
            // read ctrl point distances:
            // # Control Point No 0:  0.428994
            if (line[0] == 'C') {
                DEBUG_DEBUG(CPs.size() << " points, read: " << pnr);
                assert(pointIt == CPs.end());
                DEBUG_DEBUG("all CP errors read");
                state = 2;
                break;
            }
            if (line.find("# Control Point No") != 0) continue;
            DEBUG_DEBUG("reading cp dist line: " << line);
            string::size_type p;
            if ((p=line.find(':')) == string::npos) assert(0);
            p++;
            DEBUG_DEBUG("parsing point " << pnr << " (idx:" << p << "): " << line.substr(p));
            (*pointIt).error = utils::lexical_cast<double>(line.substr(p));
            DEBUG_DEBUG("read CP distance " << (*pointIt).error);
            pointIt++;
            pnr++;
            break;
        }
        default:
            // ignore line..
            break;
        }
    }
}

void Panorama::changeFinished()
{
    // remove change notification for nonexisting images from set.
    UIntSet::iterator uB = changedImages.lower_bound(state.images.size());
    changedImages.erase(uB,changedImages.end());

    stringstream t;
    copy(changedImages.begin(), changedImages.end(),
         ostream_iterator<unsigned int>(t, " "));
    DEBUG_TRACE("changed image(s) " << t.str() << " begin");
    std::set<PanoramaObserver *>::iterator it;
    for(it = observers.begin(); it != observers.end(); ++it) {
        DEBUG_TRACE("notifying listener");
        if (changedImages.size() > 0) {
            (*it)->panoramaImagesChanged(*this, changedImages);
        }
        (*it)->panoramaChanged(*this);
    }
    // reset changed images
    changedImages.clear();
    DEBUG_TRACE("end");
}

const Lens & Panorama::getLens(unsigned int lensNr) const
{
    assert(lensNr < state.lenses.size());
    return state.lenses[lensNr];
}


void Panorama::updateLens(unsigned int lensNr, const Lens & lens)
{
    assert(lensNr < state.lenses.size());
    state.lenses[lensNr].update(lens);
    unsigned int nImages = state.images.size();
    // flag affected images as changed
    for (unsigned int i=0; i<nImages; i++) {
        if (state.images[i].getLensNr() == lensNr) {
            imageChanged(i);
        }
    }
}

void Panorama::updateLensVariable(unsigned int lensNr, const LensVariable &var)
{
    DEBUG_TRACE("lens " << lensNr << " variable: " << var.getName());
    DEBUG_ASSERT(lensNr < state.lenses.size());

    map_get(state.lenses[lensNr].variables, var.getName()) = var;
    unsigned int nImages = state.images.size();
    for (unsigned int i=0; i<nImages; i++) {
        if (state.images[i].getLensNr() == lensNr) {
            // FIXME check for if really changed?
            imageChanged(i);
            map_get(state.variables[i], var.getName()).setValue(var.getValue());
        }
    }
}

void Panorama::setLens(unsigned int imgNr, unsigned int lensNr)
{
    DEBUG_TRACE("img: " << imgNr << "  lens:" << lensNr);
    assert(lensNr < state.lenses.size());
    assert(imgNr < state.images.size());
    state.images[imgNr].setLensNr(lensNr);
    imageChanged(imgNr);
    // copy the whole lens settings into the image
    copyLensVariablesToImage(imgNr);
    // FIXME: check if we overwrote the last instance of another lens
}

void Panorama::removeLens(unsigned int lensNr)
{
    DEBUG_ASSERT(lensNr < state.lenses.size());
    // it is an error to remove all lenses.
    DEBUG_ASSERT(state.images.size() == 0 || lensNr > 0);
    for (unsigned int i = 0; i < state.images.size(); i++) {
        if (state.images[i].getLensNr() == lensNr) {
            state.images[i].setLensNr(0);
            copyLensVariablesToImage(i);
            imageChanged(i);
        }
    }
}

unsigned int Panorama::addLens(const Lens & lens)
{
    state.lenses.push_back(lens);
    return state.lenses.size() - 1;
}

void Panorama::setMemento(PanoramaMemento & memento)
{
    DEBUG_TRACE("");
    // remove old content.
    reset();
    DEBUG_DEBUG("nr of images in memento:" << memento.images.size());

    state = memento;
    unsigned int nNewImages = state.images.size();
    DEBUG_DEBUG("nNewImages:" << nNewImages);

    // send changes for all images
    for (unsigned int i = 0; i < nNewImages; i++) {
        imageChanged(i);
    }
}

PanoramaMemento Panorama::getMemento(void) const
{
    return PanoramaMemento(state);
}


void Panorama::setOptions(const PanoramaOptions & opt)
{
    state.options = opt;

    // enforce a valid field of view
    double maxh = 0;
    double maxv = 0;
    switch (state.options.projectionFormat) {
    case PanoramaOptions::RECTILINEAR:
        maxh = 180;
        maxv = 180;
    case PanoramaOptions::CYLINDRICAL:
    case PanoramaOptions::EQUIRECTANGULAR:
        maxh = 360;
        maxv = 180;
    }
    if (state.options.HFOV > maxh)
        state.options.HFOV = maxh;
    if (state.options.HFOV > maxv)
        state.options.HFOV = maxv;
}

void Panorama::addObserver(PanoramaObserver * o)
{
    observers.insert(o);
}

bool Panorama::removeObserver(PanoramaObserver * o)
{
    return observers.erase(o) > 0;
}

void Panorama::clearObservers()
{
    observers.clear();
}

void Panorama::imageChanged(unsigned int imgNr)
{
    DEBUG_TRACE("adding image " << imgNr);
    changedImages.insert(imgNr);
    assert(changedImages.find(imgNr) != changedImages.end());
}


//==== internal function for variable & lens management

// update the variables of a Lens, when it has been changed.

void Panorama::copyLensVariablesToImage(unsigned int imgNr)
{
    DEBUG_ASSERT(imgNr < state.images.size());
    unsigned int lensNr = state.images[imgNr].getLensNr();
    DEBUG_ASSERT(lensNr < state.lenses.size());
    const Lens & lens = state.lenses[lensNr];
    for (LensVarMap::const_iterator it = lens.variables.begin();
         it != lens.variables.end();++it)
    {
        map_get(state.variables[imgNr], it->first).setValue(it->second.getValue());
    }
}
