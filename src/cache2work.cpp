/*
 * Copyright © (2011) Institut national de l'information
 *                    géographique et forestière
 *
 * Géoportail SAV <contact.geoservices@ign.fr>
 *
 * This software is a computer program whose purpose is to publish geographic
 * data using OGC WMS and WMTS protocol.
 *
 * This software is governed by the CeCILL-C license under French law and
 * abiding by the rules of distribution of free software.  You can  use,
 * modify and/ or redistribute the software under the terms of the CeCILL-C
 * license as circulated by CEA, CNRS and INRIA at the following URL
 * "http://www.cecill.info".
 *
 * As a counterpart to the access to the source code and  rights to copy,
 * modify and redistribute granted by the license, users are provided only
 * with a limited warranty  and the software's author,  the holder of the
 * economic rights,  and the successive licensors  have only  limited
 * liability.
 *
 * In this respect, the user's attention is drawn to the risks associated
 * with loading,  using,  modifying and/or developing or reproducing the
 * software by the user in light of its specific status of free software,
 * that may mean  that it is complicated to manipulate,  and  that  also
 * therefore means  that it is reserved for developers  and  experienced
 * professionals having in-depth computer knowledge. Users are therefore
 * encouraged to load and test the software's suitability as regards their
 * requirements in conditions enabling the security of their systems and/or
 * data to be ensured and,  more generally, to use and operate it in the
 * same conditions as regards security.
 *
 * The fact that you are presently reading this means that you have had
 *
 * knowledge of the CeCILL-C license and that you accept its terms.
 */

/**
 * \file cache2work.cpp
 * \author Institut national de l'information géographique et forestière
 * \~french \brief Convertit une image d'une pyramide ROK4 (TIFF tuilé) en une image TIFF, lisible avec LibtiffImage
 * \~french \details Certains formats sont propres aux spécifications d'une pyramide d'images ROK4 (format PNG).
 * Vision libimage : Rok4Image -> FileImage
 * \~english \brief Convert a ROK4 pyramide's image (tiled TIFF) to a TIFF image, readable with LibtiffImage
 */

#include <cstdlib>
#include <iostream>
#include <string.h>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
namespace logging = boost::log;
namespace keywords = boost::log::keywords;
#include <curl/curl.h>

#include <rok4/enums/Format.h>
#include <rok4/utils/Cache.h>
#include <rok4/image/file/Rok4Image.h>
#include <rok4/image/file/FileImage.h>

#include "config.h"

/** \~french Message d'usage de la commande cache2work */
std::string help = std::string("\ncache2work version ") + std::string(VERSION) + "\n\n"

    "Convert a ROK4 pyramid's TIFF image to untiled TIFF image\n\n" 

    "Usage: cache2work <INPUT FILE / OBJECT> [-c <VAL>] <OUTPUT FILE>\n\n" 

    "Parameters:\n"
    "     -h display this output"
    "     -c output compression : default value : none\n"
    "             raw     no compression\n"
    "             none    no compression\n"
    "             jpg     Jpeg encoding (quality 75)\n"
    "             jpg90   Jpeg encoding (quality 90)\n"
    "             lzw     Lempel-Ziv & Welch encoding\n"
    "             pkb     PackBits encoding\n"
    "             zip     Deflate encoding\n"
    "    -d debug logger activation\n\n"

    "Input file / object format : [ceph|s3|swift]://tray_name/object_name or [file|ceph|s3|swift]://file_name or file_name\n\n"
    
    "Example\n"
    "     cache2work file://JpegTiled.tif -c zip ZipUntiled.tif\n";

/**
 * \~french
 * \brief Affiche l'utilisation et les différentes options de la commande cache2work #help
 * \details L'affichage se fait dans le niveau de logger INFO
 */
void usage() {
    BOOST_LOG_TRIVIAL(info) << help;
}

/**
 * \~french
 * \brief Affiche un message d'erreur, l'utilisation de la commande et sort en erreur
 * \param[in] message message d'erreur
 * \param[in] error_code code de retour
 */
void error ( std::string message, int error_code ) {
    BOOST_LOG_TRIVIAL(error) <<  message ;
    usage();
    exit ( error_code );
}

/**
 ** \~french
 * \brief Fonction principale de l'outil createNodata
 * \details Tout est contenu dans cette fonction.
 * \param[in] argc nombre de paramètres
 * \param[in] argv tableau des paramètres
 * \return code de retour, 0 en cas de succès, -1 sinon
 ** \~english
 * \brief Main function for tool createNodata
 * \details All instructions are in this function.
 * \param[in] argc parameters number
 * \param[in] argv parameters array
 * \return return code, 0 if success, -1 otherwise
 */
int main ( int argc, char **argv )
{

    char* input = 0, *output = 0;
    Compression::eCompression compression = Compression::NONE;
    bool debug_logger=false;

    /* Initialisation des Loggers */
    boost::log::core::get()->set_filter( boost::log::trivial::severity >= boost::log::trivial::info );
    logging::add_common_attributes();
    boost::log::register_simple_formatter_factory< boost::log::trivial::severity_level, char >("Severity");
    logging::add_console_log (
        std::cout,
        keywords::format = "%Severity%\t%Message%"
    );

    for ( int i = 1; i < argc; i++ ) {

        if ( argv[i][0] == '-' ) {
            switch ( argv[i][1] ) {
            case 'h': // help
                usage();
                exit ( 0 );
                break;
            case 'd': // debug logs
                debug_logger = true;
                break;
            case 'c': // compression
                if ( ++i == argc ) {
                    error("Error in -c option", -1);
                }
                if ( strncmp ( argv[i], "none",4 ) == 0 || strncmp ( argv[i], "raw",3 ) == 0 ) {
                    compression = Compression::NONE;
                } else if ( strncmp ( argv[i], "jpg90",5 ) == 0 ) {
                    compression = Compression::JPEG90;
                } else if ( strncmp ( argv[i], "jpg",3 ) == 0 ) {
                    compression = Compression::JPEG;
                } else if ( strncmp ( argv[i], "lzw",3 ) == 0 ) {
                    compression = Compression::LZW;
                } else if ( strncmp ( argv[i], "zip",3 ) == 0 ) {
                    compression = Compression::DEFLATE;
                } else if ( strncmp ( argv[i], "pkb",3 ) == 0 ) {
                    compression = Compression::PACKBITS;
                } else {
                    error ( "Unknown compression : " + std::string(argv[i]), -1 );
                }
                break;
            default:
                error ( "Unknown option : -" + argv[i][1] ,-1 );
            }
        } else {
            if ( input == 0 ) input = argv[i];
            else if ( output == 0 ) output = argv[i];
            else {
                error ("Argument must specify ONE input file/object and ONE output file", -1);
            }
        }
    }

    if (debug_logger) {
        // le niveau debug du logger est activé
        boost::log::core::get()->set_filter( boost::log::trivial::severity >= boost::log::trivial::debug );
    }

    if ( input == 0 || output == 0 ) {
        error ("Argument must specify one input file/object and one output file", -1);
    }

    ContextType::eContextType type;
    std::string fo_name = std::string(input);
    std::string tray_name;

    ContextType::split_path(fo_name, type, fo_name, tray_name);

    Context* context;
    curl_global_init(CURL_GLOBAL_ALL);

    BOOST_LOG_TRIVIAL(debug) <<  std::string("Input is on a " + ContextType::to_string(type) + " storage in the tray ") + tray_name;
    context = StoragePool::get_context(type, tray_name);

    // Problème lors de l'ajout ou de la récupération de ce contexte de stockage
    if (context == NULL) {
        error("Unable to connect context", -1);
    }

    Rok4Image* rok4_image = Rok4Image::create_to_read(fo_name, BoundingBox<double>(0.,0.,0.,0.), 0., 0., context);
    if (rok4_image == NULL) {
        StoragePool::clean_storages();
        error (std::string("Cannot create ROK4 image to read ") + input, 1);
    }

    FileImage* output_image = FileImage::create_to_write(
        output, rok4_image->get_bbox(), rok4_image->get_resx(), rok4_image->get_resy(), rok4_image->get_width(), rok4_image->get_height(),
        rok4_image->get_channels(), rok4_image->get_sample_format(), rok4_image->get_photometric(), compression
    );

    if (output_image == NULL) {
        delete rok4_image;
        StoragePool::clean_storages();
        error (std::string("Cannot create image to write ") + output, -1);
    }

    BOOST_LOG_TRIVIAL(debug) <<  "Write" ;
    if (output_image->write_image(rok4_image) < 0) {
        delete rok4_image;
        delete output_image;
        StoragePool::clean_storages();
        error("Cannot write image", -1);
    }

    BOOST_LOG_TRIVIAL(debug) <<  "Clean" ;
    
    delete rok4_image;
    delete output_image;
    ProjPool::clean_projs();
    proj_cleanup();
    CurlPool::clean_curls();
    curl_global_cleanup();
    StoragePool::clean_storages();

    return 0;
}
