// -*- c-basic-offset: 4 -*-
/** @file simplestitcher.h
 *
 *  Contains various routines used for stitching panoramas.
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

#ifndef _SIMPLESTITCHER_H
#define _SIMPLESTITCHER_H

#include <vector>
#include <utility>

#include <vigra/impex.hxx>

//#include <PT/SpaceTransform.h>
#include <PT/tiffUtils.h>

namespace PT{

typedef std::vector<std::pair<float, unsigned int> > AlphaVector;

/** calculate weight factors for seaming
 *
 *  \warning modifies \p distance
 */
static inline void seamFactor(AlphaVector & distance,
               float seamWidth,
               AlphaVector & weights)
{
    // sort distance.
    sort(distance.begin(), distance.end());

    float distSum=0;
    float min=distance[0].first;
    weights.clear();
    if (seamWidth <= 0 ) {
        // special case for seamWidth = zero
        weights.push_back(std::make_pair(1, distance[0].second));
        return;
    }

    // walk over distance, add points that need to be added
    for (AlphaVector::const_iterator it = distance.begin();
         it != distance.end(); ++it)
    {
        double d = it->first - min;
        if ( d <= seamWidth) {
            // seam width.
            distSum += seamWidth - d;
            weights.push_back(std::make_pair(seamWidth - d, it->second));
        }
    }
    for (AlphaVector::iterator it = weights.begin();
         it != weights.end(); ++it)
    {
        it->first = it->first / distSum;
    }
}

static inline void addTiffTags(vigra::TiffImage * tiff, const std::string & filename,
                               const std::string & basename,
                               uint16 page, uint16 nImg,
                               vigra::Diff2D offset)
{
            // create a new directory for our image
            // hopefully I didn't forget too much stuff..
            TIFFCreateDirectory (tiff);

            // set page
            TIFFSetField (tiff, TIFFTAG_SUBFILETYPE, FILETYPE_PAGE);
            TIFFSetField (tiff, TIFFTAG_PAGENUMBER, (unsigned short)page, (unsigned short)nImg);

            // resolution to 1, to avoid rounding errors of buggy tif loaders.
            TIFFSetField (tiff, TIFFTAG_XRESOLUTION, (float) 1);
            TIFFSetField (tiff, TIFFTAG_YRESOLUTION, (float) 1);
            // offsets must allways be positive so correct them
            TIFFSetField (tiff, TIFFTAG_XPOSITION, (float)(offset.x));
            TIFFSetField (tiff, TIFFTAG_YPOSITION, (float)(offset.y));

            // save input name.
            TIFFSetField (tiff, TIFFTAG_DOCUMENTNAME, basename.c_str());
            TIFFSetField (tiff, TIFFTAG_PAGENAME,filename.c_str() );
            //
            TIFFSetField (tiff, TIFFTAG_IMAGEDESCRIPTION, "created with nona (http://hugin.sf.net)");
}

/** stitch a panorama
 *
 * @todo more advanced seam calculation?
 * @todo move seam calculation into a separate class/function?
 * @todo usage of different iterpolators
 * @todo vignetting correction
 * @todo do not keep all images in memory, if short on mem.
 * @todo proper handling for 16 bit images etc.
 *
 */
template <class DestImageType>
void stitchPanoramaSimple(const PT::Panorama & pano,
                          const PT::PanoramaOptions & opts,
                          DestImageType & dest,
                          utils::MultiProgressDisplay & progress,
                          const std::string & basename,
                          const std::string & format = "tif",
                          bool savePartial = false)
{
    // until we have something better...
    typedef DestImageType InputImageType;
    typedef DestImageType OutputImageType;

    typedef
        vigra::NumericTraits<typename OutputImageType::Accessor::value_type> DestTraits;

    DEBUG_ASSERT(pano.getNrOfImages() > 0);

    float seamWidth = pano.getImage(0).getOptions().featherWidth;
    DEBUG_DEBUG("using seamWidth: " << seamWidth);

    // resize dest to output panorama size
    dest.resize(opts.width, opts.getHeight());

    // catch the errors that can be thrown by vigra.. easier to debug then...

    // remapped panorama images
    std::vector<RemappedPanoImage<OutputImageType, vigra::FImage> > remapped(pano.getNrOfImages());


    // remap all images (keep in memory...)
    unsigned int nImg = pano.getNrOfImages();

    progress.pushTask(utils::ProgressTask("Stitching", "", 1.0/(nImg+1)));

    for (unsigned int i=0; i< nImg; i++) {
        // load image
        const PT::PanoImage & img = pano.getImage(i);
        vigra::ImageImportInfo info(img.getFilename().c_str());
        // create a RGB image of appropriate size
        // FIXME.. use some other mechanism to define what format to use..
        InputImageType srcImg(info.width(), info.height());
        // import the image just read
        progress.setMessage("loading image " + img.getFilename());
        importImage(info, destImage(srcImg));

        progress.setMessage("remapping " + img.getFilename());

        // this should be made a bit smarter, but I don't
        // want to have virtual function call for the interpolator
        switch (opts.interpolator) {
        case PT::PanoramaOptions::CUBIC:
            DEBUG_DEBUG("using cubic interpolator");
            PT::remapImage(pano, i,
                           srcImageRange(srcImg),
                           opts,
                           remapped[i],
                           interp_cubic(),
                           progress);
            break;
        case PT::PanoramaOptions::SPLINE_16:
            DEBUG_DEBUG("interpolator: spline16");
            PT::remapImage(pano, i,
                           srcImageRange(srcImg),
                           opts,
                           remapped[i],
                           interp_spline16(),
                           progress);
            break;
        case PT::PanoramaOptions::SPLINE_36:
            DEBUG_DEBUG("interpolator: spline36");
            PT::remapImage(pano, i,
                           srcImageRange(srcImg),
                           opts,
                           remapped[i],
                           interp_spline36(),
                           progress);
            break;
        case PT::PanoramaOptions::SPLINE_64:
            DEBUG_DEBUG("interpolator: spline64");
            PT::remapImage(pano, i,
                           srcImageRange(srcImg),
                           opts,
                           remapped[i],
                           interp_spline64(),
                           progress);
            break;
        case PT::PanoramaOptions::SINC_256:
            DEBUG_DEBUG("interpolator: sinc 256")
            PT::remapImage(pano, i,
                           srcImageRange(srcImg),
                           opts,
                           remapped[i],
                           interp_sinc<8>(),
                           progress);
            break;
        case PT::PanoramaOptions::BILINEAR:
            PT::remapImage(pano, i,
                           srcImageRange(srcImg),
                           opts,
                           remapped[i],
                           interp_bilin(),
                           progress);
            break;
        case PT::PanoramaOptions::NEAREST_NEIGHBOUR:
            PT::remapImage(pano, i,
                           srcImageRange(srcImg),
                           opts,
                           remapped[i],
                           interp_nearest(),
                           progress);
            break;
        case PT::PanoramaOptions::SINC_1024:
            PT::remapImage(pano, i,
                           srcImageRange(srcImg),
                           opts,
                           remapped[i],
                           interp_sinc<32>(),
                           progress);
            break;
        }

        // test
        if ( savePartial) {
            // write out the destination images
            std::ostringstream ofname;
            ofname << basename << "_" << i << ".tif";
            exportImage(srcImageRange(remapped[i].image), vigra::ImageExportInfo(ofname.str().c_str()));
            std::ostringstream ofdistname;
            ofdistname << basename << "_dist_" << i << ".tif";
            exportImage(srcImageRange(remapped[i].dist), vigra::ImageExportInfo(ofdistname.str().c_str()));
        }
    }

    DEBUG_DEBUG("merging images");
    // stitch images
    progress.pushTask(utils::ProgressTask("Flattening", ""));

    // save individual images into a single big multi-image tif
    if (opts.outputFormat == PT::PanoramaOptions::TIFF_mask ||
        opts.outputFormat == PT::PanoramaOptions::TIFF_m )
    {
        std::string filename = basename + ".tif";
        DEBUG_DEBUG("Layering image into a multi image tif file " << filename);
        vigra::TiffImage * tiff = TIFFOpen(filename.c_str(), "w");
        DEBUG_ASSERT(tiff);

        // test for widest negative offset as it is not allowed in tiff
        int neg_off_x = 0,
            neg_off_y = 0;
        for (unsigned int imgNr=0; imgNr< nImg; imgNr++) {
            if (remapped[imgNr].ul.x < neg_off_x)
                neg_off_x = remapped[imgNr].ul.x;
            if (remapped[imgNr].ul.y < neg_off_y)
                neg_off_y = remapped[imgNr].ul.y;
        }

        // loop over all images and create alpha channel for it,
        // and write it into the output file.
        for (unsigned int imgNr=0; imgNr< nImg; imgNr++) {

            std::ostringstream tstr("Image: ");
            tstr << imgNr;
            progress.setMessage(tstr.str());

            vigra::Diff2D sz = remapped[imgNr].image.size();
            vigra::BImage alpha(sz);

            // over whole image
            int xstart = remapped[imgNr].ul.x;
            int xend = remapped[imgNr].ul.x + sz.x;
            int ystart = remapped[imgNr].ul.y;
            int yend = remapped[imgNr].ul.y + sz.y;

            // write default tiff tags for this layer
            vigra::Diff2D offset(xstart - neg_off_x, ystart-neg_off_y);
            addTiffTags(tiff, pano.getImage(imgNr).getFilename(), basename,
                        imgNr+1, nImg, offset);


            if (opts.outputFormat == PT::PanoramaOptions::TIFF_m) {
                vigra::BImage::Iterator ya(alpha.upperLeft());
                for(int y=ystart; y < yend; ++y, ya.y++) {
                    // create x iterators
                    typename vigra::BImage::Iterator xa(ya);
                    for(int x=xstart; x < xend; ++x, ++xa.x) {
                        // find the image where this pixel is closest to the image center
                        vigra::Diff2D cp(x,y);
                        if (remapped[imgNr].getDistanceFromCenter(cp) != FLT_MAX) {
                            *xa=255;
                        } else {
                            *xa=0;
                        }
                    }
                    if ((yend-y) % ((yend-ystart)/20) == 0) {
                        progress.setProgress(1.0*(y-ystart)/((yend-ystart)/20)/nImg);
                    }
                }
            } else {
                vigra::BImage::Iterator ya(alpha.upperLeft());
                for(int y=ystart; y < yend; ++y, ya.y++) {
                    // create x iterators
                    typename vigra::BImage::Iterator xa(ya);
                    for(int x=xstart; x < xend; ++x, ++xa.x) {
                        // find the image where this pixel is closest to the image center
                        vigra::Diff2D cp(x,y);

                        float minDist = FLT_MAX;

                        float topDist = FLT_MAX;
                        float topImgNr = 0;

                        for (unsigned int i=0; i< nImg; i++) {
                            float dist = remapped[i].getDistanceFromCenter(cp);
                            // minimum distance
                            if ( dist < minDist ) {
                                minDist = dist;
                            }
                            // distance in topmost layer.
                            if (dist < FLT_MAX) {
                                topDist = dist;
                                topImgNr = i;
                            }
                        }

                        // feather only topmost layer
                        if (topImgNr == imgNr) {
                            topDist = remapped[imgNr].getDistanceFromCenter(cp);
                            float diff = topDist - minDist;
                            if (diff > 0 && diff <= seamWidth) {
                                *xa = (unsigned char) (255 * ( (seamWidth - diff)/seamWidth) +0.5);
                            } else if (diff > seamWidth || topDist == FLT_MAX) {
                                *xa = 0;
                            } else {
                                *xa = 255;
                            }
                        } else if (remapped[imgNr].getDistanceFromCenter(cp) < FLT_MAX) {
                            *xa = 255;
                        } else {
                            *xa = 0;
                        }
                    }
                    if ((yend-y) % ((yend-ystart)/20) == 0) {
                        progress.setProgress(1.0*(y-ystart)/((yend-ystart)/20)/nImg);
                    }
                }
            }
            // call vigra function to write the image data
            vigra::createBRGBATiffImage(remapped[imgNr].image.upperLeft(),
                                        remapped[imgNr].image.lowerRight(),
                                        remapped[imgNr].image.accessor(),
                                        alpha.upperLeft(), alpha.accessor(),
                                        tiff);
            // write this image to disk
            TIFFWriteDirectory (tiff);
            TIFFFlushData (tiff);
        }
        TIFFClose(tiff);
    } else {
        // flatten image into a panorama image
        DEBUG_DEBUG("flattening image");
        typename OutputImageType::Accessor oac(dest.accessor());

        typedef typename
            vigra::NumericTraits<typename OutputImageType::Accessor::value_type>::RealPromote SumType;
        // over whole image
        int xstart = 0;
        int xend = dest.width();
        int ystart = 0;
        int yend = dest.height();


        // loop over whole image
        typename DestImageType::Iterator yd(dest.upperLeft());
        for(int y=ystart; y < yend; ++y, ++yd.y)
        {
            // create x iterators
            typename DestImageType::Iterator xd(yd);

            AlphaVector dists(nImg);
            AlphaVector alpha(nImg);
            for(int x=xstart; x < xend; ++x, ++xd.x)
            {
                // find the image where this pixel is closest to the image center
                vigra::Diff2D cp(x,y);
                dists.clear();
                for (unsigned int i=0; i< nImg; i++) {
                    float dist = remapped[i].getDistanceFromCenter(cp);
                    if ( dist < FLT_MAX ) {
                        dists.push_back(std::make_pair(dist,i));
                    }
                }
                if (dists.size() > 0) {
                    // if image is defind in this area.
                    // use float for alpha merging
                    SumType blended;

                    // use a simple weighted sum, to merge the images.
                    seamFactor(dists, seamWidth, alpha);
                    for (AlphaVector::iterator it = alpha.begin();
                         it != alpha.end(); ++it)
                    {
                        blended = blended + it->first *  remapped[it->second].get(cp);
                    }
                    *xd = DestTraits::fromRealPromote(blended);
                }
            }
            if ((yend-y) % ((yend-ystart)/20) == 0) {
                DEBUG_DEBUG("progress update: " << ((double)y-ystart)/(yend-ystart));
                progress.setProgress(((double)y-ystart)/(yend-ystart));
            }
        }

        // save final panorama
        std::string filename = basename + "." + format;
	DEBUG_DEBUG("saving output file: " << filename);
        vigra::ImageExportInfo exinfo(filename.c_str());
        if (format == "jpg") {
            std::ostringstream jpgqual;
            jpgqual << opts.quality;
            exinfo.setCompression(jpgqual.str().c_str());
        }
        exportImage(srcImageRange(dest), exinfo);
    }
    progress.popTask();
    progress.popTask();


}

} // namespace PTools

#endif // _SIMPLESTITCHER_H
