// -*- c-basic-offset: 4 -*-
/** @file 
 *
* !! from PTOptimise.h 1951
 *
 *  functions to call the optimizer of panotools.
 *
 *  @author Pablo d'Angelo <pablo.dangelo@web.de>
 *
 *  $Id: PTOptimise.h 1951 2007-04-15 20:54:49Z dangelo $
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

#include "PTOptimizer.h"

#include "ImageGraph.h"
#include <panotools/PanoToolsOptimizerWrapper.h>
#include <algorithms/basic/CalculateCPStatistics.h>
#include <algorithms/nona/CenterHorizontally.h>
#include <algorithms/nona/CalculateFOV.h>

#if DEBUG
#include <fstream>
#include <boost/graph/graphviz.hpp>
#endif

namespace HuginBase {
    
    
bool PTOptimizer::runAlgorithm()
{
    PTools::optimize(o_panorama);
    return true; // let's hope so.
}
    

void AutoOptimise::autoOptimise(PanoramaData& pano)
    
{
    // DGSW FIXME - Unreferenced
    //	unsigned nImg = unsigned(pano.getNrOfImages());
    // build a graph over all overlapping images
    CPGraph graph;
    createCPGraph(pano,graph);
    
#if DEBUG
    {
        std::ofstream gfile("cp_graph.dot");
        // output doxygen graph
        boost::write_graphviz(gfile, graph);
    }
#endif
    std::set<std::string> optvars;
    optvars.insert("r");
    optvars.insert("p");
    optvars.insert("y");
    
    unsigned int startImg = pano.getOptions().optimizeReferenceImage;
    
    // start a breadth first traversal of the graph, and optimize
    // the links found (every vertex just once.)
    
    OptimiseVisitor optVisitor(pano, optvars);
    
    boost::queue<boost::graph_traits<CPGraph>::vertex_descriptor> qu;
    boost::breadth_first_search(graph, startImg,
                                color_map(get(boost::vertex_color, graph)).
                                visitor(optVisitor));
    /*
#ifdef DEBUG
     // print optimized script to cout
     DEBUG_DEBUG("after local optim:");
     VariableMapVector vars = optVisitor.getVariables();
     for (unsigned v=0; v < pano.getNrOfImages(); v++) {
         printVariableMap(std::cerr, vars[v]);
         std::cerr << std::endl;
     }
#endif
     
     // apply variables to input panorama
     pano.updateVariables(optVisitor.getVariables());
     
#ifdef DEBUG
     UIntSet allImg;
     fill_set(allImg,0, pano.getNrOfImages()-1);
     // print optimized script to cout
     DEBUG_DEBUG("after updateVariables():");
     pano.printPanoramaScript(std::cerr, pano.getOptimizeVector(), pano.getOptions(), allImg, false);
#endif
     */
}


void SmartOptimise::smartOptimize(PanoramaData& optPano)
{
    // use m-estimator with sigma 2
    PanoramaOptions opts = optPano.getOptions();
    double oldSigma = opts.huberSigma;
    opts.huberSigma = 2;
    optPano.setOptions(opts);
    
    // remove vertical and horizontal control points
    CPVector cps = optPano.getCtrlPoints();
    CPVector newCP;
    for (CPVector::const_iterator it = cps.begin(); it != cps.end(); it++) {
        if (it->mode == ControlPoint::X_Y)
        {
            newCP.push_back(*it);
        }
    }
    optPano.setCtrlPoints(newCP);
    AutoOptimise::autoOptimise(optPano);
    
    // do global optimisation of position with all control points.
    optPano.setCtrlPoints(cps);
    OptimizeVector optvars = createOptVars(optPano, OPT_POS);
    optPano.setOptimizeVector(optvars);
    PTools::optimize(optPano);
    
    //---------------------------------------------------------------
    // Now with lens distortion
    
    // force inherit for all d/e values
    char varnames[] = "abcde";
    char vname[2];
    vname[1] = 0;
    for (unsigned i=0; i< optPano.getNrOfLenses(); i++) {
        for(char * v=varnames; (*v) != 0; v++) {
            vname[0] = *v;
            LensVariable lv = const_map_get(optPano.getLens(i).variables, vname);
            lv.setLinked(true);
            optPano.updateLensVariable(i, lv);
        }
    }
    
    int optmode = OPT_POS;
    double origHFOV = const_map_get(optPano.getImageVariables(0),"v").getValue();
    
    // determine which parameters to optimize
    double rmin, rmax, rmean, rvar, rq10, rq90;
    CalculateCPStatisticsRadial::calcCtrlPntsRadiStats(optPano, rmin, rmax, rmean, rvar, rq10, rq90);
    
    DEBUG_DEBUG("Ctrl Point radi statistics: min:" << rmin << " max:" << rmax << " mean:" << rmean << " var:" << rvar << " q10:" << rq10 << " q90:" << rq90);
    
    if (origHFOV > 60) {
        // only optimize principal point if the hfov is high enough for sufficient perspective effects
        optmode |= OPT_DE;
    }
    
    // heuristics for distortion and fov optimisation
    if ( (rq90 - rq10) >= 1.2) {
        // very well distributed control points 
        // TODO: other criterion when to optimize HFOV, too
        optmode |= OPT_AC | OPT_B;
    } else if ( (rq90 - rq10) > 1.0) {
        optmode |= OPT_AC | OPT_B;
    } else {
        optmode |= OPT_B;
    }
    
    // check if this is a 360 deg pano.
    CenterHorizontally(optPano).run();
    FDiff2D fov = CalculateFOV(optPano).run<CalculateFOV>().getResultFOV();
    
    if (fov.x >= 359) {
        // optimize HFOV for 360 deg panos
        optmode |= OPT_HFOV;
    }
    
    DEBUG_DEBUG("second optimization: " << optmode);
    
    // save old pano, might be needed if optimization went wrong
    UIntSet allImgs;
    fill_set(allImgs, 0, optPano.getNrOfImages()-1);
    PanoramaData& oldPano = *(optPano.getNewSubset(allImgs)); // don't forget to delete
    optvars = createOptVars(optPano, optmode);
    optPano.setOptimizeVector(optvars);
    // global optimisation.
    PTools::optimize(optPano);
    
    // --------------------------------------------------------------
    // do some plausibility checks and reoptimize with less variables
    // if something smells fishy
    bool smallHFOV = false;
    bool highDist = false;
    bool highShift = false;
    const VariableMapVector & vars = optPano.getVariables();
    for (VariableMapVector::const_iterator it = vars.begin() ; it != vars.end(); it++)
    {
        if (const_map_get(*it,"v").getValue() < 1.0) smallHFOV = true;
        if (fabs(const_map_get(*it,"a").getValue()) > 0.8) highDist = true;
        if (fabs(const_map_get(*it,"b").getValue()) > 0.8) highDist = true;
        if (fabs(const_map_get(*it,"c").getValue()) > 0.8) highDist = true;
        if (fabs(const_map_get(*it,"d").getValue()) > 2000) highShift = true;
        if (fabs(const_map_get(*it,"e").getValue()) > 2000) highShift = true;
    }
    
    if (smallHFOV || highDist || highShift) {
        DEBUG_DEBUG("Optimization with strange result. status: HFOV: " << smallHFOV << " dist:" << highDist << " shift:" << highShift);
        // something seems to be wrong
        if (smallHFOV) {
            // do not optimize HFOV
            optmode &= ~OPT_HFOV;
        }
        if (highDist) {
            optmode &= ~OPT_AC;
        }
        if (highShift) {
            optmode &= ~OPT_DE;
        }
        
        // revert and redo optimisation
        optPano = oldPano;
        optvars = createOptVars(optPano, optmode);
        optPano.setOptimizeVector(optvars);
        DEBUG_DEBUG("recover optimisation: " << optmode);
        // global optimisation.
        PTools::optimize(optPano);
    }
    opts.huberSigma = oldSigma;
    optPano.setOptions(opts);
    
    delete &oldPano;
}

OptimizeVector SmartOptimizerStub::createOptVars(const PanoramaData& optPano, int mode, unsigned anchorImg)
{
    OptimizeVector optvars;
    for (unsigned i=0; i < optPano.getNrOfImages(); i++) {
        std::set<std::string> imgopt;
        // do not optimize anchor image for position and exposure
        if (i!=anchorImg) {
            if (mode & OPT_POS) {
                imgopt.insert("r");
                imgopt.insert("p");
                imgopt.insert("y");
            }
            if (mode & OPT_EXP) {
                imgopt.insert("Eev");
            }
            if (mode & OPT_WB) {
                imgopt.insert("Er");
                imgopt.insert("Eb");
            }
        }
        if (mode & OPT_HFOV) {
            imgopt.insert("v");
        }
        if (mode & OPT_B)
            imgopt.insert("b");
        if (mode & OPT_AC) {
            imgopt.insert("a");
            imgopt.insert("c");
        }
        if (mode & OPT_DE) {
            imgopt.insert("d");
            imgopt.insert("e");
        }
        if (mode & OPT_GT) {
            imgopt.insert("g");
            imgopt.insert("t");
        }
        if (mode & OPT_VIG) {
            imgopt.insert("Vb");
            imgopt.insert("Vc");
            imgopt.insert("Vd");
        }
        if (mode & OPT_VIGCENTRE) {
            imgopt.insert("Vx");
            imgopt.insert("Vy");
        }
        if (mode & OPT_RESP) {
            imgopt.insert("Ra");
            imgopt.insert("Rb");
            imgopt.insert("Rc");
            imgopt.insert("Rd");
            imgopt.insert("Re");
        }
        optvars.push_back(imgopt);
    }
    
    return optvars;
}

} //namespace
