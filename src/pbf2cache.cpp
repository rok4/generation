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
 * \file pbf2cache.cpp
 * \author Institut national de l'information géographique et forestière
 * \~french \brief Consitution d'une dalle vecteur ROK4 à partir des tuiles PBF
 * \~english \brief Build a vector ROK4 slab from PBD tiles
 */

#include <cstdlib>
#include <iostream>
#include <string.h>
#include <tiffio.h>
#include <rok4/enums/Format.h>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
namespace logging = boost::log;
namespace keywords = boost::log::keywords;

#include <rok4/storage/Context.h>
#include <rok4/image/file/FileImage.h>
#include <rok4/utils/Cache.h>
#include <rok4/image/file/Rok4Image.h>
#include "config.h"

/** \~french Message d'usage de la commande pbf2cache */
std::string help = std::string("\npbf2cache version ") + std::string(VERSION) + "\n\n"

    "Make image tiled and compressed, in TIFF format, respecting ROK4 specifications.\n\n"

    "Usage: pbf2cache -r <DIRECTORY> -t <VAL> <VAL> -ultile <VAL> <VAL> <OUTPUT FILE / OBJECT> [-d]\n\n"

    "Parameters:\n"
    "     -r directory containing the PBF tiles : tile I,J is stored to path <DIRECTORY>/I/J.pbf\n"
    "     -t number of tiles in the slab : widthwise and heightwise.\n"
    "     -ultile upper left tile indices\n"
    "     -d debug logger activation\n\n"

    "Output file / object format : [ceph|s3|swift]://tray_name/object_name or [file|ceph|s3|swift]://file_name or file_name\n\n";

/**
 * \~french
 * \brief Affiche l'utilisation et les différentes options de la commande pbf2cache #help
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
 * \brief Fonction principale de l'outil pbf2cache
 * \param[in] argc nombre de paramètres
 * \param[in] argv tableau des paramètres
 * \return code de retour, 0 en cas de succès, -1 sinon
 ** \~english
 * \brief Main function for tool pbf2cache
 * \param[in] argc parameters number
 * \param[in] argv parameters array
 * \return return code, 0 if success, -1 otherwise
 */
int main ( int argc, char **argv ) {

    char* output = 0, *root_directory = 0;
    int tiles_per_width = 16, tiles_per_height = 16;
    int upper_left_column = -1;
    int upper_left_row = -1;

    bool debug_logger = false;

    /* Initialisation des Loggers */
    boost::log::core::get()->set_filter( boost::log::trivial::severity >= boost::log::trivial::info );
    logging::add_common_attributes();
    boost::log::register_simple_formatter_factory< boost::log::trivial::severity_level, char >("Severity");
    logging::add_console_log (
        std::cout,
        keywords::format = "%Severity%\t%Message%"
    );

    // Récupération des paramètres
    for ( int i = 1; i < argc; i++ ) {

        if ( !strcmp ( argv[i],"-ultile" ) ) {
            if ( i+2 >= argc ) { error("Error in -ultile option", -1 ); }
            upper_left_column = atoi ( argv[++i] );
            upper_left_row = atoi ( argv[++i] );
            continue;
        }

        if ( argv[i][0] == '-' ) {
            switch ( argv[i][1] ) {
                case 'h': // help
                    usage();
                    exit ( 0 );
                case 'd': // debug logs
                    debug_logger = true;
                    break;
                case 'r': // root directory
                    if ( i++ >= argc ) {
                        BOOST_LOG_TRIVIAL(error) <<  "Error in option -r" ;
                        return -1;
                    }
                    root_directory = argv[i];
                    break;
                case 't':
                    if ( i+2 >= argc ) { error("Error in -t option", -1 ); }
                    tiles_per_width = atoi ( argv[++i] );
                    tiles_per_height = atoi ( argv[++i] );
                    break;

                default:
                    error ( "Unknown option : " + std::string(argv[i]) ,-1 );
            }
        } else {
            if ( output == 0 ) output = argv[i];
            else { error ( "Argument must specify ONE output file/object", -1 ); }
        }
    }

    if (debug_logger) {
        // le niveau debug du logger est activé
        boost::log::core::get()->set_filter( boost::log::trivial::severity >= boost::log::trivial::debug );
    }

    if ( root_directory == 0 || output == 0 ) {
        error ("Argument must specify one output file/object and one root directory", -1);
    }

    BOOST_LOG_TRIVIAL(debug) << "Output : " << output;
    BOOST_LOG_TRIVIAL(debug) << "PBF root directory : " << root_directory;

    if ( upper_left_row == -1 || upper_left_column == -1 ) {
        error ("Upper left tile indices have to be provided (with option -ultile)", -1);
    }

    ContextType::eContextType type;
    std::string fo_name = std::string(output);
    std::string tray_name;

    ContextType::split_path(fo_name, type, fo_name, tray_name);

    Context* context;
    curl_global_init(CURL_GLOBAL_ALL);

    BOOST_LOG_TRIVIAL(debug) <<  std::string("Output is on a " + ContextType::to_string(type) + " storage in the tray ") + tray_name;
    context = StoragePool::get_context(type, tray_name);

    Rok4Image* rok4_image = Rok4Image::create_to_write( fo_name, tiles_per_width, tiles_per_height, context );
    
    if (rok4_image == NULL) {
        error("Cannot create the ROK4 image to write", -1);
    }

    if (debug_logger) {
        rok4_image->print();
    }

    BOOST_LOG_TRIVIAL(debug) <<  "Write" ;

    if (rok4_image->writePbfTiles(upper_left_column, upper_left_row, root_directory) < 0) {
        error("Cannot write ROK4 image from PBF tiles", -1);
    }

    BOOST_LOG_TRIVIAL(debug) <<  "Clean" ;

    CrsBook::clean_crss();
    ProjPool::clean_projs();
    proj_cleanup();
    CurlPool::clean_curls();
    curl_global_cleanup();
    StoragePool::clean_storages();
    delete rok4_image;

    return 0;
}
