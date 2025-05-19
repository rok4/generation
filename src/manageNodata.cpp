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
 * \file manageNodata.cpp
 * \author Institut national de l'information géographique et forestière
 * \~french \brief Gère la couleur des pixels de nodata
 * \~english \brief Manage nodata pixel color
 * \~french \details Cet outil est destiné à :
 * \li identifier les pixels de nodata à partir d'une valeur et d'une tolérance
 * \li modifier les pixels qui contiennent cette valeur
 * \li écrire le masque de données associé à l'image
 *
 * L'outil gère les images à canaux entiers non signés sur 8 bits ou flottant sur 32 bits.
 *
 * Les paramètres sont les suivants :
 *
 * \li l'image en entrée (obligatoire)
 * \li l'image en sortie, au format TIFF. Si on ne la précise pas, les éventuelles modifications de l'image écraseront l'image source.
 * \li le masque de sortie, au format TIFF. (pas écrit si non précisé).
 *
 * Dans le cas où seul le masque associé nous intéresse, on ne réecrira jamais de nouvelle image, même si un chemin de sortie différent de l'entrée était précisé.
 *
 * Si l'image traitée ne contient pas de nodata, le masque n'est pas écrit. En effet, on considère une image sans masque associé comme une image pleine.
 *
 * On peut également définir 3 couleurs :
 * \li la couleur cible (obligatoire) : les pixels de cette couleur sont ceux potentiellement considérés comme du nodata et modifiés. On peut également préciser une tolérance en complément de la valeur. L'option "touche les bords" précise la façon dont on identifie les pixels de nodata.
 * \li la nouvelle couleur de nodata : si elle n'est pas précisée, cela veut dire qu'on ne veut pas la modifier
 * \li la nouvelle couleur de donnée : si elle n'est pas précisée, cela veut dire qu'on ne veut pas la modifier
 *
 * \~ \image html manageNodata.png \~french
 *
 * Cet outil n'est qu'une interface permettant l'utilisation de la classe TiffNodataManager, qui réalise réellement tous les traitements.
 *
 * Le nombre de canaux du fichier en entrée et les valeurs de nodata renseignée doivent être cohérents.
 */

using namespace std;

#include <rok4/enums/Format.h>
#include <rok4/utils/Cache.h>
#include <rok4/image/file/TiffNodataManager.h>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
namespace logging = boost::log;
namespace keywords = boost::log::keywords;

#include <cstdlib>
#include <iostream>
#include <string.h>
#include "config.h"

/** \~french Message d'usage de la commande manageNodata */
std::string help = std::string("\nmanageNodata version ") + std::string(VERSION) + "\n\n"

        "Manage nodata pixel color in a TIFF file, byte samples\n\n"

        "Usage: manageNodata -target <VAL> [-tolerance <VAL>] [-touch-edges] -format <VAL> [-nodata <VAL>] [-data <VAL>] <INPUT FILE> [<OUTPUT FILE>] [-mask-out <VAL>]\n\n"

        "Colors are provided in decimal format, one integer value per sample\n"
        "Parameters:\n"
        "      -target         color to consider as nodata / modify\n"
        "      -tolerance      a positive integer, to define a delta for target value's comparison\n"
        "      -touche-edges   method to identify nodata pixels (all 'target value' pixels or just those at the edges\n"
        "      -data           new color for data pixel which contained target color\n"
        "      -nodata         new color for nodata pixel\n"
        "      -mask-out       path to the mask to write\n"
        "      -format         image's samples' format : uint8 or float32\n"
        "      -channels       samples per pixel,number of samples in provided colors\n"
        "      -d              debug logger activation\n\n"

        "Examples :\n"
        "      - to keep pure white for nodata, and write a new image :\n"
        "              manageNodata -target 255,255,255 -touch-edges -data 254,254,254 input_image.tif output_image.tif -channels 3 -format uint8\n"
        "      - to write the associated mask (all '-99999' pixels are nodata, with a tolerance):\n"
        "              manageNodata -target -99999 -tolerance 10 input_image.tif -mask-out mask.tif -channels 1 -format float32\n\n";
    
/**
 * \~french
 * \brief Affiche l'utilisation et les différentes options de la commande manageNodata #help
 * \details L'affichage se fait dans la sortie d'erreur
 */
void usage() {
    BOOST_LOG_TRIVIAL(info) << help;
}

/**
 * \~french
 * \brief Affiche un message d'erreur, l'utilisation de la commande et sort en erreur
 * \param[in] message message d'erreur
 * \param[in] error_code code de retour, -1 par défaut
 */
void error ( std::string message, int error_code = -1 ) {
    BOOST_LOG_TRIVIAL(error) <<  message ;
    usage();
    exit ( error_code );
}

/**
 ** \~french
 * \brief Fonction principale de l'outil manageNodata
 * \details Tout est contenu dans cette fonction.
 * \param[in] argc nombre de paramètres
 * \param[in] argv tableau des paramètres
 * \return code de retour, 0 en cas de succès, -1 sinon
 ** \~english
 * \brief Main function for tool manageNodata
 * \details All instructions are in this function.
 * \param[in] argc parameters number
 * \param[in] argv parameters array
 * \return return code, 0 if success, -1 otherwise
 */
int main ( int argc, char* argv[] ) {
    char* input_image_path = 0;
    char* output_image_path = 0;

    char* output_mask_path = 0;

    char* target_value_string = 0;
    char* new_nodata_value_string = 0;
    char* new_data_value_string = 0;

    int channels = 0;
    SampleFormat::eSampleFormat sample_format = SampleFormat::UNKNOWN;

    bool touch_edges = false;
    int tolerance = 0;
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

        if ( !strcmp ( argv[i],"-h" ) ) {
            usage();
            exit ( 0 ) ;
        }
        
        if ( !strcmp ( argv[i],"-d" ) ) { // debug logs
            debug_logger = true;
            break;
        }

        if ( !strcmp ( argv[i],"-touch-edges" ) ) {
            touch_edges = true;
            continue;

        } else if ( !strcmp ( argv[i],"-tolerance" ) ) {
            if ( i++ >= argc ) error ( "Error with option -tolerance",-1 );
            tolerance = atoi ( argv[i] );
            if ( tolerance < 0 ) error ( "Error with option -tolerance : have to be a positive integer",-1 );
            continue;

        } else if ( !strcmp ( argv[i],"-target" ) ) {
            if ( i++ >= argc ) error ( "Error with option -target",-1 );
            target_value_string = argv[i];
            continue;

        } else if ( !strcmp ( argv[i],"-nodata" ) ) {
            if ( i++ >= argc ) error ( "Error with option -nodata",-1 );
            new_nodata_value_string = argv[i];
            continue;

        } else if ( !strcmp ( argv[i],"-data" ) ) {
            if ( i++ >= argc ) error ( "Error with option -data",-1 );
            new_data_value_string = argv[i];
            continue;

        } else if ( !strcmp ( argv[i],"-format" ) ) {
            if ( i++ >= argc ) error ( "Error with option -format",-1 );
            if ( strncmp ( argv[i], "uint8", 5 ) == 0 ) {
                sample_format = SampleFormat::UINT8;
            } else if ( strncmp ( argv[i], "float32", 7 ) == 0 ) {
                sample_format = SampleFormat::FLOAT32;
            } else error ( "Unknown value for option -format : " + string ( argv[i] ), -1 );
            continue;

        } else if ( !strcmp ( argv[i],"-channels" ) ) {
            if ( i++ >= argc ) error ( "Error with option -channels",-1 );
            channels = atoi ( argv[i] );
            continue;

        } else if ( !strcmp ( argv[i],"-mask-out" ) ) {
            if ( i++ >= argc ) error ( "Error with option -mask-out",-1 );
            output_mask_path = argv[i];
            continue;

        } else if ( !input_image_path ) {
            input_image_path = argv[i];

        } else if ( !output_image_path ) {
            output_image_path = argv[i];

        } else {
            error ( "Error : unknown option : " + string ( argv[i] ),-1 );
        }
    }

    if (debug_logger) {
        // le niveau debug du logger est activé
        boost::log::core::get()->set_filter( boost::log::trivial::severity >= boost::log::trivial::debug );
    }

    /***************** VERIFICATION DES PARAMETRES FOURNIS *********************/

    if ( ! input_image_path ) error ( "Missing input file",-1 );

    bool overwrite = false;
    if ( ! output_image_path ) {
        BOOST_LOG_TRIVIAL(info) <<  "If the input image have to be modify, it will be overwrite" ;
        output_image_path = new char[sizeof ( input_image_path ) +1];
        overwrite = true;
        memcpy ( output_image_path, input_image_path, sizeof ( input_image_path ) );
    }

    if ( ! channels ) error ( "Missing number of samples per pixel",-1 );
    if ( sample_format == SampleFormat::UNKNOWN ) error ( "Missing sample format",-1 );

    if ( ! target_value_string )
        error ( "How to identify the nodata in the input image ? Provide a target color (-target)",-1 );

    if ( ! new_nodata_value_string && ! new_data_value_string && ! output_mask_path )
        error ( "What have we to do with the target color ? Precise a new nodata or data color, or a mask to write",-1 );

    int* target_value = new int[channels];
    int* new_nodata_value = new int[channels];
    int* new_data_value = new int[channels];

    /***************** INTERPRETATION DES COULEURS FOURNIES ********************/
    BOOST_LOG_TRIVIAL(debug) <<  "Color interpretation" ;

    // Target value
    char* char_iterator = strtok ( target_value_string,"," );
    if ( char_iterator == NULL ) {
        error ( "Error with option -target : integer values seperated by comma",-1 );
    }
    target_value[0] = atoi ( char_iterator );
    for ( int i = 1; i < channels; i++ ) {
        char_iterator = strtok ( NULL, "," );
        if ( char_iterator == NULL ) {
            error ( "Error with option -oldValue : integer values seperated by comma",-1 );
        }
        target_value[i] = atoi ( char_iterator );
    }

    // New nodata
    if ( new_nodata_value_string ) {
        char_iterator = strtok ( new_nodata_value_string,"," );
        if ( char_iterator == NULL ) {
            error ( "Error with option -nodata : integer values seperated by comma",-1 );
        }
        new_nodata_value[0] = atoi ( char_iterator );
        for ( int i = 1; i < channels; i++ ) {
            char_iterator = strtok ( NULL, "," );
            if ( char_iterator == NULL ) {
                error ( "Error with option -nodata : integer values seperated by comma",-1 );
            }
            new_nodata_value[i] = atoi ( char_iterator );
        }
    } else {
        // On ne précise pas de nouvelle couleur de non-donnée, elle est la même que la couleur cible.
        memcpy ( new_nodata_value, target_value, channels*sizeof ( int ) );
    }

    // New data
    if ( new_data_value_string ) {
        char_iterator = strtok ( new_data_value_string,"," );
        if ( char_iterator == NULL ) {
            error ( "Error with option -data : integer values seperated by comma",-1 );
        }
        new_data_value[0] = atoi ( char_iterator );
        for ( int i = 1; i < channels; i++ ) {
            char_iterator = strtok ( NULL, "," );
            if ( char_iterator == NULL ) {
                error ( "Error with option -data : integer values seperated by comma",-1 );
            }
            new_data_value[i] = atoi ( char_iterator );
        }
    } else {
        // Pas de nouvelle couleur pour la donnée : elle a la valeur de la couleur cible
        memcpy ( new_data_value, target_value, channels*sizeof ( int ) );
    }

    /******************* APPEL A LA CLASSE TIFFNODATAMANAGER *******************/

    if ( sample_format == SampleFormat::FLOAT32 ) {
        BOOST_LOG_TRIVIAL(debug) <<  "Target color treatment (uint8)" ;
        TiffNodataManager<float> TNM ( channels, target_value, touch_edges, new_data_value, new_nodata_value, tolerance );
        if ( ! TNM.process_nodata ( input_image_path, output_image_path, output_mask_path ) ) {
            error ( "Error : unable to treat nodata for this 32-bit float image : " + string ( input_image_path ), -1 );
        }
    } else if ( sample_format == SampleFormat::UINT8 ) {
        BOOST_LOG_TRIVIAL(debug) <<  "Target color treatment (float)" ;
        TiffNodataManager<uint8_t> TNM ( channels, target_value, touch_edges, new_data_value, new_nodata_value, tolerance );
        if ( ! TNM.process_nodata ( input_image_path, output_image_path, output_mask_path ) ) {
            error ( "Error : unable to treat nodata for this 8-bit integer float image : " + string ( input_image_path ), -1 );
        }
    }

    BOOST_LOG_TRIVIAL(debug) <<  "Clean" ;

    CrsBook::clean_crss();
    ProjPool::clean_projs();
    proj_cleanup();
    delete[] target_value;
    if (overwrite) delete[] output_image_path;
    delete[] new_data_value;
    delete[] new_nodata_value;

    return 0;
}
