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
 * \file work2cache.cpp
 * \author Institut national de l'information géographique et forestière
 * \~french \brief Formattage d'une image, respectant les spécifications d'une pyramide de données ROK4
 * \~english \brief Image formatting, according to ROK4 specifications
 * \~french \details Le serveur ROK4 s'attend à trouver dans une pyramide d'images des images au format TIFF, tuilées, potentiellement compressées, avec une en-tête de taille fixe (2048 octets). C'est cet outil, via l'utilisation de la classe TiledTiffWriter, qui va "mettre au normes" les images.
 *
 * Vision libimage : FileImage -> Rok4Image
 */

#include <cstdlib>
#include <iostream>
#include <fstream>
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

/** \~french Message d'usage de la commande work2cache */
std::string help = std::string("\nwork2cache version ") + std::string(VERSION) + "\n\n"

    "Make image tiled and compressed, in TIFF format, respecting ROK4 specifications.\n\n"

    "Usage: work2cache -c <VAL> -t <VAL> <VAL> <INPUT FILE> <OUTPUT FILE / OBJECT>\n\n"

    "Parameters:\n"
    "     -c output compression :\n"
    "             raw     no compression\n"
    "             none    no compression\n"
    "             jpg     Jpeg encoding (quality 75)\n"
    "             jpg90   Jpeg encoding (quality 90)\n"
    "             lzw     Lempel-Ziv & Welch encoding\n"
    "             pkb     PackBits encoding\n"
    "             zip     Deflate encoding\n"
    "             png     Non-official TIFF compression, each tile is an independant PNG image (with PNG header)\n"
    "     -t tile size : widthwise and heightwise. Have to be a divisor of the global image's size\n"
    "     -a sample format : (float32 or uint8)\n"
    "     -s samples per pixel : (1, 2, 3 or 4)\n"
    "     -d : debug logger activation\n\n"

    "If sampleformat or samplesperpixel are not provided, those informations are read from the image sources (all have to own the same). If all are provided, conversion may be done.\n\n"

    "Output file / object format : [ceph|s3|swift]://tray_name/object_name or [file|ceph|s3|swift]://file_name or file_name\n\n"
    
    "Examples\n"
    "     - for orthophotography\n"
    "     work2cache input.tif -c png -t 256 256 output.tif\n"
    "     - for DTM\n"
    "     work2cache input.tif -c zip -t 256 256 output.tif\n\n";

/**
 * \~french
 * \brief Affiche l'utilisation et les différentes options de la commande work2cache #help
 * \details L'affichage se fait dans le niveau de logger INFO
 */
void usage() {
    BOOST_LOG_TRIVIAL(info) <<  (help);
}

/**
 * \~french
 * \brief Affiche un message d'erreur, l'utilisation de la commande et sort en erreur
 * \param[in] message message d'erreur
 * \param[in] error_code code de retour
 */
void error ( std::string message, int error_code ) {
    BOOST_LOG_TRIVIAL(error) <<  ( message );
    usage();
    exit ( error_code );
}

/**
 ** \~french
 * \brief Fonction principale de l'outil work2cache
 * \details Tout est contenu dans cette fonction. Le tuilage / compression est géré Rok4Image
 * \param[in] argc nombre de paramètres
 * \param[in] argv tableau des paramètres
 * \return code de retour, 0 en cas de succès, -1 sinon
 ** \~english
 * \brief Main function for tool work2cache
 * \details All instructions are in this function. Rok4Image make image tiled and compressed.
 * \param[in] argc parameters number
 * \param[in] argv parameters array
 * \return return code, 0 if success, -1 otherwise
 */
int main ( int argc, char **argv ) {

    char* input_path = 0, *output_path = 0;
    int tile_width = 256, tile_height = 256;
    Compression::eCompression compression = Compression::NONE;

    bool output_format_provided = false;
    uint16_t samplesperpixel = 0;
    SampleFormat::eSampleFormat sampleformat = SampleFormat::UNKNOWN;

    Photometric::ePhotometric photometric;

    bool debug_logger=false;

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

        if ( argv[i][0] == '-' ) {
            switch ( argv[i][1] ) {
                case 'h': // help
                    usage();
                    exit ( 0 );
                case 'd': // debug logs
                    debug_logger = true;
                    break;
                case 'c': // compression
                    if ( ++i == argc ) { error ( "Error in -c option", -1 ); }
                    if ( strncmp ( argv[i], "none",4 ) == 0 || strncmp ( argv[i], "raw",3 ) == 0 ) {
                        compression = Compression::NONE;
                    } else if ( strncmp ( argv[i], "png",3 ) == 0 ) {
                        compression = Compression::PNG;
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
                case 't':
                    if ( i+2 >= argc ) { error("Error in -t option", -1 ); }
                    tile_width = atoi ( argv[++i] );
                    tile_height = atoi ( argv[++i] );
                    break;

                /****************** OPTIONNEL, POUR FORCER DES CONVERSIONS **********************/
                case 's': // samplesperpixel
                    if ( i++ >= argc ) {
                        error ( "Error in option -s", -1 );
                    }
                    if ( strncmp ( argv[i], "1",1 ) == 0 ) samplesperpixel = 1 ;
                    else if ( strncmp ( argv[i], "2",1 ) == 0 ) samplesperpixel = 2 ;
                    else if ( strncmp ( argv[i], "3",1 ) == 0 ) samplesperpixel = 3 ;
                    else if ( strncmp ( argv[i], "4",1 ) == 0 ) samplesperpixel = 4 ;
                    else {
                        error ( "Unknown value for option -s : " + std::string(argv[i]), -1 );
                    }
                    break;
                case 'a': // sampleformat
                    if ( i++ >= argc ) {
                        error ( "Error in option -a", -1 );
                    }
                    if ( strncmp ( argv[i],"uint8",5 ) == 0 ) sampleformat = SampleFormat::UINT8 ;
                    else if ( strncmp ( argv[i],"float32",7 ) == 0 ) sampleformat = SampleFormat::FLOAT32;
                    else {
                        error ( "Unknown value for option -a : " + std::string(argv[i]), -1 );
                    }
                    break;
                /*******************************************************************************/

                default:
                    error ( "Unknown option : " + std::string(argv[i]) ,-1 );
            }
        } else {
            if ( input_path == 0 ) input_path = argv[i];
            else if ( output_path == 0 ) output_path = argv[i];
            else { error ( "Argument must specify ONE input file and ONE output file/object", 2 ); }
        }
    }

    if (debug_logger) {
        // le niveau debug du logger est activé
        boost::log::core::get()->set_filter( boost::log::trivial::severity >= boost::log::trivial::debug );
    }

    if ( input_path == 0 || output_path == 0 ) {
        error ("Argument must specify one input file and one output file/object", -1);
    }

    ContextType::eContextType type;
    std::string fo_name = std::string(output_path);
    std::string tray_name;

    ContextType::split_path(fo_name, type, fo_name, tray_name);

    Context* context;
    curl_global_init(CURL_GLOBAL_ALL);
    
    BOOST_LOG_TRIVIAL(debug) <<  std::string("Output is on a " + ContextType::to_string(type) + " storage in the tray ") + tray_name;
    context = StoragePool::get_context(type, tray_name);

    BOOST_LOG_TRIVIAL(debug) <<  ( "Open image to read" );
    FileImage* source_image = FileImage::create_to_read(input_path);
    if (source_image == NULL) {
        error("Cannot read the source image", -1);
    }

    // On regarde si on a tout précisé en sortie, pour voir si des conversions sont demandées et possibles
    if (sampleformat != SampleFormat::UNKNOWN && samplesperpixel !=0) {
        output_format_provided = true;
        // La photométrie est déduite du nombre de canaux
        if (samplesperpixel == 1) {
            photometric = Photometric::GRAY;
        } else if (samplesperpixel == 2) {
            photometric = Photometric::GRAY;
        } else {
            photometric = Photometric::RGB;
        }

        if (! source_image->add_converter ( sampleformat, samplesperpixel ) ) {
            error ( "Cannot add converter to the input FileImage " + std::string(input_path), -1 );
        }
    } else {
        // On n'a pas précisé de format de sortie
        // La sortie aura ce format
        photometric = source_image->get_photometric();
        sampleformat = source_image->get_sample_format();
        samplesperpixel = source_image->get_channels();
    }
    
    if (debug_logger) {
        source_image->print();
    }

    Rok4Image* rok4_image = Rok4Image::create_to_write(
        fo_name, BoundingBox<double>(0.,0.,0.,0.), -1, -1, source_image->get_width(), source_image->get_height(), samplesperpixel,
        sampleformat, photometric, compression,
        tile_width, tile_height, context
    );
    
    if (rok4_image == NULL) {
        error("Cannot create the ROK4 image to write", -1);
    }

    rok4_image->set_extra_sample(source_image->get_extra_sample());

    if (debug_logger) {
        rok4_image->print();
    }

    BOOST_LOG_TRIVIAL(debug) <<  ( "Write" );

    if (rok4_image->write_image(source_image) < 0) {
        error("Cannot write ROK4 image", -1);
    }

    BOOST_LOG_TRIVIAL(debug) <<  ( "Clean" );

    CrsBook::clean_crss();
    ProjPool::clean_projs();
    proj_cleanup();
    CurlPool::clean_curls();
    curl_global_cleanup();
    StoragePool::clean_storages();
    delete source_image;
    delete rok4_image;

    return 0;
}
