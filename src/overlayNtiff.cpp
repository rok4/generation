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
 * \file overlayNtiff.cpp
 * \author Institut national de l'information géographique et forestière
 * \~french \brief Fusion de N images aux mêmes dimensions, selon différentes méthodes
 * \~english \brief Merge N images with same dimensions, according to different merge methods
 */

#include <iostream>
#include <cstdlib>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <algorithm>
#include <string>
#include <fstream>
#include <tiffio.h>
#include <tiff.h>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
namespace logging = boost::log;
namespace keywords = boost::log::keywords;

#include <rok4/image/file/FileImage.h>
#include <rok4/image/MergeImage.h>
#include <rok4/enums/Format.h>
#include <math.h>
#include "config.h"
#include <rok4/utils/Cache.h>

/** \~french Chemin du fichier de configuration des images */
char configuration_path[256];
/** \~french Nombre de canaux par pixel de l'image en sortie */
uint16_t samplesperpixel = 0;
/** \~french Format du canal (entier, flottant, signé ou non...), dans les images en entrée et celle en sortie */
SampleFormat::eSampleFormat sampleformat;
/** \~french Photométrie (rgb, gray), pour les images en sortie */
Photometric::ePhotometric photometric = Photometric::RGB;
/** \~french Compression de l'image de sortie */
Compression::eCompression compression = Compression::NONE;
/** \~french Mode de fusion des images */
Merge::eMergeType merge_method = Merge::UNKNOWN;

/** \~french Couleur à considérer comme transparent dans le images en entrée. Vrai couleur (sur 3 canaux). Peut ne pas être définie */
int* transparent;
/** \~french Couleur à utiliser comme fond. Doit comporter autant de valeur qu'on veut de canaux dans l'image finale. Si un canal alpha est présent, il ne doit pas être prémultiplié aux autres canaux */
int* background;

/** \~french Activation du niveau de log debug. Faux par défaut */
bool debug_logger=false;


/** \~french Message d'usage de la commande overlayNtiff */
std::string help = std::string("\noverlayNtiff version ") + std::string(VERSION) + "\n\n"

    "Create one TIFF image, from several images with same dimensions, with different available merge methods.\n"
    "Sources and output image can have different numbers of samples per pixel. The sample type have to be the same for all sources and will be the output one\n\n"

    "Usage: overlayNtiff -f <FILE> -m <VAL> -c <VAL> -s <VAL> -p <VAL [-n <VAL>] -b <VAL>\n"

    "Parameters:\n"
    "    -f configuration file : list of output and source images and masks\n"
    "    -c output compression :\n"
    "            raw     no compression\n"
    "            none    no compression\n"
    "            jpg     Jpeg encoding (quality 75)\n"
    "            jpg90   Jpeg encoding (quality 90)\n"
    "            lzw     Lempel-Ziv & Welch encoding\n"
    "            pkb     PackBits encoding\n"
    "            zip     Deflate encoding\n"
    "    -t value to consider as transparent, 3 integers, separated with comma. Optionnal\n"
    "    -b value to use as background, one integer per output sample, separated with comma\n"
    "    -m merge method : used to merge input images, associated masks are always used if provided :\n"
    "            ALPHATOP       images are merged by alpha blending\n"
    "            MULTIPLY       samples are multiplied one by one\n"
    "            TOP            only the top data pixel is kept\n"
    "    -s output samples per pixel : 1, 2, 3 or 4\n"
    "    -p output photometric :\n"
    "            gray    min is black\n"
    "            rgb     for image with alpha too\n"
    "    -d debug logger activation\n\n"

    "Examples\n"
    "    - for gray orthophotography, with transparency (white is transparent)\n"
    "    overlayNtiff -f conf.txt -m ALPHATOP -s 1 -c zip -p gray -t 255,255,255 -b 0\n"
    "    - for DTM, considering masks only\n"
    "    overlayNtiff -f conf.txt -m TOP -s 1 -c zip -p gray -b -99999\n\n";

/**
 * \~french
 * \brief Affiche l'utilisation et les différentes options de la commande overlayNtiff #help
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
    BOOST_LOG_TRIVIAL(error) <<  "Configuration file : " << configuration_path ;
    usage();
    exit ( error_code );
}

/**
 * \~french
 * \brief Récupère les valeurs passées en paramètres de la commande, et les stocke dans les variables globales
 * \param[in] argc nombre de paramètres
 * \param[in] argv tableau des paramètres
 * \return code de retour, 0 si réussi, -1 sinon
 */
int parse_command_line ( int argc, char** argv ) {

    char transparent_string[256];
    memset ( transparent_string, 0, 256 );
    char background_string[256];
    memset ( background_string, 0, 256 );

    for ( int i = 1; i < argc; i++ ) {
        if ( argv[i][0] == '-' ) {
            switch ( argv[i][1] ) {
            case 'h': // help
                usage();
                exit ( 0 );
            case 'd': // debug logs
                debug_logger = true;
                break;
            case 'f': // Images' list file
                if ( i++ >= argc ) {
                    BOOST_LOG_TRIVIAL(error) <<  "Error with images' list file (option -f)" ;
                    return -1;
                }
                strcpy ( configuration_path,argv[i] );
                break;
            case 'm': // image merge method
                if ( i++ >= argc ) {
                    BOOST_LOG_TRIVIAL(error) <<  "Error with merge method (option -m)" ;
                    return -1;
                }
                merge_method = Merge::from_string ( argv[i] );
                if ( merge_method == Merge::UNKNOWN ) {
                    BOOST_LOG_TRIVIAL(error) <<  "Unknown value for merge method (option -m) : " << argv[i] ;
                    return -1;
                }
                break;
            case 's': // samplesperpixel
                if ( i++ >= argc ) {
                    BOOST_LOG_TRIVIAL(error) <<  "Error with samples per pixel (option -s)" ;
                    return -1;
                }
                if ( strncmp ( argv[i], "1",1 ) == 0 ) samplesperpixel = 1 ;
                else if ( strncmp ( argv[i], "2",1 ) == 0 ) samplesperpixel = 2 ;
                else if ( strncmp ( argv[i], "3",1 ) == 0 ) samplesperpixel = 3 ;
                else if ( strncmp ( argv[i], "4",1 ) == 0 ) samplesperpixel = 4 ;
                else {
                    BOOST_LOG_TRIVIAL(error) <<  "Unknown value for samples per pixel (option -s) : " << argv[i] ;
                    return -1;
                }
                break;
            case 'c': // compression
                if ( i++ >= argc ) {
                    BOOST_LOG_TRIVIAL(error) <<  "Error with compression (option -c)" ;
                    return -1;
                }
                if ( strncmp ( argv[i], "raw",3 ) == 0 ) compression = Compression::NONE;
                else if ( strncmp ( argv[i], "none",4 ) == 0 ) compression = Compression::NONE;
                else if ( strncmp ( argv[i], "zip",3 ) == 0 ) compression = Compression::DEFLATE;
                else if ( strncmp ( argv[i], "pkb",3 ) == 0 ) compression = Compression::PACKBITS;
                else if ( strncmp ( argv[i], "jpg90",5 ) == 0 ) compression = Compression::JPEG90;
                else if ( strncmp ( argv[i], "jpg",3 ) == 0 ) compression = Compression::JPEG;
                else if ( strncmp ( argv[i], "lzw",3 ) == 0 ) compression = Compression::LZW;
                else {
                    BOOST_LOG_TRIVIAL(error) <<  "Unknown value for compression (option -c) : " << argv[i] ;
                    return -1;
                }
                break;
            case 'p': // photometric
                if ( i++ >= argc ) {
                    BOOST_LOG_TRIVIAL(error) <<  "Error with photometric (option -p)" ;
                    return -1;
                }
                if ( strncmp ( argv[i], "gray",4 ) == 0 ) photometric = Photometric::GRAY;
                else if ( strncmp ( argv[i], "rgb",3 ) == 0 ) photometric = Photometric::RGB;
                else {
                    BOOST_LOG_TRIVIAL(error) <<  "Unknown value for photometric (option -p) : " << argv[i] ;
                    return -1;
                }
                break;
            case 't': // transparent color
                if ( i++ >= argc ) {
                    BOOST_LOG_TRIVIAL(error) <<  "Error with transparent color (option -t)" ;
                    return -1;
                }
                strcpy ( transparent_string,argv[i] );
                break;
            case 'b': // background color
                if ( i++ >= argc ) {
                    BOOST_LOG_TRIVIAL(error) <<  "Error with background color (option -b)" ;
                    return -1;
                }
                strcpy ( background_string,argv[i] );
                break;
            default:
                BOOST_LOG_TRIVIAL(error) <<  "Unknown option : -" << argv[i][1] ;
                return -1;
            }
        }
    }

    // Merge method control
    if ( merge_method == Merge::UNKNOWN ) {
        BOOST_LOG_TRIVIAL(error) <<  "We need to know the merge method (option -m)" ;
        return -1;
    }

    // Image list file control
    if ( strlen ( configuration_path ) == 0 ) {
        BOOST_LOG_TRIVIAL(error) <<  "We need to have one images' list (text file, option -f)" ;
        return -1;
    }

    // Samples per pixel control
    if ( samplesperpixel == 0 ) {
        BOOST_LOG_TRIVIAL(error) <<  "We need to know the number of samples per pixel in the output image (option -s)" ;
        return -1;
    }

    if (merge_method == Merge::ALPHATOP && strlen ( transparent_string ) != 0 ) {
        transparent = new int[3];

        // Transparent interpretation
        char* char_iterator = strtok ( transparent_string,"," );
        if ( char_iterator == NULL ) {
            BOOST_LOG_TRIVIAL(error) <<  "Error with option -t : 3 integers values separated by comma" ;
            return -1;
        }
        int value = atoi ( char_iterator );
        transparent[0] = value;
        for ( int i = 1; i < 3; i++ ) {
            char_iterator = strtok ( NULL, "," );
            if ( char_iterator == NULL ) {
                BOOST_LOG_TRIVIAL(error) <<  "Error with option -t : 3 integers values separated by comma" ;
                return -1;
            }
            value = atoi ( char_iterator );
            transparent[i] = value;
        }
    }

    if ( strlen ( background_string ) != 0 ) {
        background = new int[samplesperpixel];

        // Background interpretation
        char* char_iterator = strtok ( background_string,"," );
        if ( char_iterator == NULL ) {
            BOOST_LOG_TRIVIAL(error) <<  "Error with option -b : one integer value per final sample separated by comma" ;
            return -1;
        }
        int value = atoi ( char_iterator );
        background[0] = value;

        for ( int i = 1; i < samplesperpixel; i++ ) {
            char_iterator = strtok ( NULL, "," );
            if ( char_iterator == NULL ) {
                BOOST_LOG_TRIVIAL(error) <<  "Error with option -b : one integer value per final sample separated by comma" ;
                return -1;
            }
            value = atoi ( char_iterator );
            background[i] = value;
        }

    } else {
        BOOST_LOG_TRIVIAL(error) <<  "We need to know the background value for the output image (option -b)" ;
        return -1;
    }

    return 0;
}

/**
 * \~french
 * \brief Lit une ligne du fichier de configuration
 * \details Une ligne contient le chemin vers une image, potentiellement suivi du chemin vers le masque associé.
 * \param[in,out] configuration_file flux de lecture vers le fichier de configuration
 * \param[out] image_path chemin de l'image lu dans le fichier de configuration
 * \param[out] has_mask précise si l'image possède un masque
 * \param[out] mask_path chemin du masque lu dans le fichier de configuration
 * \return code de retour, 0 en cas de succès, -1 si la fin du fichier est atteinte, 1 en cas d'erreur
 */
int read_configuration_line ( std::ifstream& configuration_file, char* image_path, bool* has_mask, char* mask_path ) {
    std::string str;

    while ( str.empty() ) {
        if ( configuration_file.eof() ) {
            BOOST_LOG_TRIVIAL(debug) <<  "Configuration file end reached" ;
            return -1;
        }
        std::getline ( configuration_file, str );
    }

    if ( std::sscanf ( str.c_str(),"%s %s", image_path, mask_path ) == 2 ) {
        *has_mask = true;
    } else {
        *has_mask = false;
    }

    return 0;
}

/**
 * \~french
 * \brief Charge les images en entrée et en sortie depuis le fichier de configuration
 * \details On va récupérer toutes les informations de toutes les images et masques présents dans le fichier de configuration et créer les objets LibtiffImage correspondant. Toutes les images ici manipulées sont de vraies images (physiques) dans ce sens où elles sont des fichiers soit lus, soit qui seront écrits.
 *
 * \param[out] output_image image résultante de l'outil
 * \param[out] output_mask masque résultat de l'outil, si demandé
 * \param[out] pImageIn ensemble des images en entrée
 * \return code de retour, 0 si réussi, -1 sinon
 */
int load_images ( FileImage** output_image, FileImage** output_mask, MergeImage** merged_image ) {
    char input_image_path[IMAGE_MAX_FILENAME_LENGTH];
    char input_mask_path[IMAGE_MAX_FILENAME_LENGTH];

    char output_image_path[IMAGE_MAX_FILENAME_LENGTH];
    char output_mask_path[IMAGE_MAX_FILENAME_LENGTH];

    std::vector<Image*> input_images;
    BoundingBox<double> empty_bbox ( 0.,0.,0.,0. );

    int width, height;

    bool has_input_mask, has_output_mask;

    // Ouverture du fichier texte listant les images
    std::ifstream configuration_file;

    configuration_file.open ( configuration_path );
    if ( ! configuration_file ) {
        BOOST_LOG_TRIVIAL(error) <<  "Cannot open the file " << configuration_path ;
        return -1;
    }

    // Lecture de l'image de sortie
    if ( read_configuration_line ( configuration_file,output_image_path,&has_output_mask,output_mask_path ) ) {
        BOOST_LOG_TRIVIAL(error) <<  "Cannot read output image in the file : " << configuration_path ;
        return -1;
    }

    // On doit connaître les dimensions des images en entrée pour pouvoir créer les images de sortie

    // Lecture et création des images sources
    int input_count = 0;
    int out = 0;
    while ( ( out = read_configuration_line ( configuration_file,input_image_path,&has_input_mask,input_mask_path ) ) == 0 ) {
        FileImage* input_image = FileImage::create_to_read ( input_image_path );
        if ( input_image == NULL ) {
            BOOST_LOG_TRIVIAL(error) <<  "Cannot create a FileImage from the file " << input_image_path ;
            return -1;
        }

        if ( input_count == 0 ) {
            // C'est notre première image en entrée, on mémorise les caractéristiques)
            sampleformat = input_image->get_sample_format();
            width = input_image->get_width();
            height = input_image->get_height();
        } else {
            // Toutes les images en entrée doivent avoir certaines caractéristiques en commun
            if (sampleformat != input_image->get_sample_format() || width != input_image->get_width() || height != input_image->get_height() ) {
                BOOST_LOG_TRIVIAL(error) <<  "All input images must have same dimension and sample type" ;
                return -1;
            }
        }

        if ( has_input_mask ) {
            /* On a un masque associé, on en fait une image à lire et on vérifie qu'elle est cohérentes :
             *          - même dimensions que l'image
             *          - 1 seul canal (entier)
             */
            FileImage* pMask = FileImage::create_to_read ( input_mask_path );
            if ( pMask == NULL ) {
                BOOST_LOG_TRIVIAL(error) <<  "Cannot create a FileImage (mask) from the file " << input_mask_path ;
                return -1;
            }

            if ( ! input_image->set_mask ( pMask ) ) {
                BOOST_LOG_TRIVIAL(error) <<  "Cannot add mask " << input_mask_path ;
                return -1;
            }
        }

        input_images.push_back ( input_image );
        input_count++;
    }

    if ( out != -1 ) {
        BOOST_LOG_TRIVIAL(error) <<  "Failure reading the file " << configuration_path ;
        return -1;
    }

    // Fermeture du fichier
    configuration_file.close();

    // On crée notre MergeImage, qui s'occupera des calculs de fusion des pixels

    *merged_image = MergeImage::create ( input_images, samplesperpixel, background, transparent, merge_method );

    // Le masque fusionné est ajouté
    MergeMask* merged_mask = new MergeMask ( *merged_image );

    if ( ! ( *merged_image )->set_mask ( merged_mask ) ) {
        BOOST_LOG_TRIVIAL(error) <<  "Cannot add mask to the merged image" ;
        return -1;
    }

    // Création des sorties
    *output_image = FileImage::create_to_write ( output_image_path, empty_bbox, -1., -1., width, height, samplesperpixel,
                  sampleformat, photometric,compression );

    if ( *output_image == NULL ) {
        BOOST_LOG_TRIVIAL(error) <<  "Impossible de creer l'image " << output_image_path ;
        return -1;
    }

    if ( has_output_mask ) {
        *output_mask = FileImage::create_to_write ( output_mask_path, empty_bbox, -1., -1., width, height, 1,
                     SampleFormat::UINT8, Photometric::MASK, Compression::DEFLATE );

        if ( *output_mask == NULL ) {
            BOOST_LOG_TRIVIAL(error) <<  "Impossible de creer le masque " << output_mask_path ;
            return -1;
        }
    }

    return 0;
}

/**
 ** \~french
 * \brief Fonction principale de l'outil overlayNtiff
 * \param[in] argc nombre de paramètres
 * \param[in] argv tableau des paramètres
 * \return code de retour, 0 si réussi, -1 sinon
 ** \~english
 * \brief Main function for tool overlayNtiff
 * \param[in] argc parameters number
 * \param[in] argv parameters array
 * \return 0 if success, -1 otherwise
 */
int main ( int argc, char **argv ) {

    FileImage* output_image ;
    FileImage* output_mask = NULL;
    MergeImage* merged_image;

    /* Initialisation des Loggers */
    boost::log::core::get()->set_filter( boost::log::trivial::severity >= boost::log::trivial::info );
    logging::add_common_attributes();
    boost::log::register_simple_formatter_factory< boost::log::trivial::severity_level, char >("Severity");
    logging::add_console_log (
        std::cout,
        keywords::format = "%Severity%\t%Message%"
    );

    BOOST_LOG_TRIVIAL(debug) <<  "Read parameters" ;
    // Lecture des parametres de la ligne de commande
    if ( parse_command_line ( argc,argv ) < 0 ) {
        error ( "Cannot parse command line",-1 );
    }

    // On sait maintenant si on doit activer le niveau de log DEBUG
    if (debug_logger) {
        boost::log::core::get()->set_filter( boost::log::trivial::severity >= boost::log::trivial::debug );
    }

    BOOST_LOG_TRIVIAL(debug) <<  "Load" ;
    // Chargement des images
    if ( load_images ( &output_image,&output_mask,&merged_image ) < 0 ) {
        error ( "Cannot load images from the configuration file",-1 );
    }

    BOOST_LOG_TRIVIAL(debug) <<  "Save image" ;
    // Enregistrement de l'image fusionnée
    if ( output_image->write_image ( merged_image ) < 0 ) {
        error ( "Cannot write the merged image",-1 );
    }

    // Enregistrement du masque fusionné, si demandé
    if ( output_mask != NULL) {
        BOOST_LOG_TRIVIAL(debug) <<  "Save mask" ;
        if ( output_mask->write_image ( merged_image->Image::get_mask() ) < 0 ) {
            error ( "Cannot write the merged mask",-1 );
        }
    }

    CrsBook::clean_crss();
    ProjPool::clean_projs();
    proj_cleanup();
    delete merged_image;
    delete output_image;
    delete output_mask;

    delete [] background;
    if ( transparent != NULL ) {
        delete [] transparent;
    }

    return 0;
}
