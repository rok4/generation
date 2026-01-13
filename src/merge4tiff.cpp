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
 * \page merge4tiff Commande merge4tiff
 * \author Institut national de l'information géographique et forestière
 * \~ \image html merge4tiff.png \~french
 * \~french \brief Sous echantillonage de 4 images disposées en carré, avec utilisation possible de fond et de masques de données
 * \~english \brief Four images subsampling, formed a square, might use a background and data masks
 * 
 * \~french
 * 
 * L'implémentation de cette commande se trouve dans le fichier \ref merge4tiff.cpp
 * \section diagram Détails du chaînage des différentes classes d'image :
 * 
 * @mermaid{merge4tiff}
 * 
 */

/** \file merge4tiff.cpp
 * \~french
 * \brief Fichier d'implémentation de la commande merge4tiff
 * 
 * Le fonctionnement général est décrit dans la page \ref merge4tiff .
 * 
 * \~english
 * \brief Implementation file for command merge4tiff
 * 
 * Global operation is described into page \ref merge4tiff .
 */

#include <tiffio.h>

#include <boost/algorithm/string.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
namespace logging = boost::log;
namespace keywords = boost::log::keywords;

#include <cstdlib>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <string.h>
#include <stdint.h>

#include <rok4/image/Image.h>
#include <rok4/enums/Format.h>
#include <rok4/image/file/FileImage.h>
#include <rok4/image/ExtendedCompoundImage.h>
#include <rok4/image/MergeImage.h>
#include <rok4/image/SubsampledImage.h>
#include "config.h"

/* Valeurs de nodata */
/** \~french Valeur de nodata sour forme de chaîne de caractère (passée en paramètre de la commande) */
char* strnodata = 0;

/* Chemins des images en entrée et en sortie */
/** \~french Chemin de l'image de fond */
std::string background_image_path;
/** \~french Chemin du masque associé à l'image de fond */
std::string background_mask_path;
/** \~french Chemins des images en entrée */
std::vector<std::string> input_images_paths = {"", "", "", ""};
;
/** \~french Chemins des masques associés aux images en entrée */
std::vector<std::string> input_masks_paths = {"", "", "", ""};
/** \~french Chemin de l'image en sortie */
std::string output_image_path;
/** \~french Chemin du masque associé à l'image en sortie */
std::string output_mask_path;

/* Caractéristiques des images en entrée et en sortie */
/** \~french Valeur de gamma, pour foncer ou éclaircir des images en entier */
double local_gamma = 1.;
/** \~french Largeur des images */
uint32_t width;
/** \~french Hauteur des images */
uint32_t height;
/** \~french Compression de l'image de sortie */
Compression::eCompression compression = Compression::NONE;

/** \~french A-t-on précisé le format en sortie, c'est à dire les 3 informations samplesperpixel et sampleformat */
bool output_format_provided = false;
/** \~french Nombre de canaux par pixel, pour l'image en sortie */
uint16_t samplesperpixel = 0;
/** \~french Format du canal (entier, flottant, signé ou non...), pour l'image en sortie */
SampleFormat::eSampleFormat sampleformat = SampleFormat::UNKNOWN;

/** \~french Photométrie (rgb, gray), déduit du nombre de canaux */
Photometric::ePhotometric photometric;

/** \~french Activation du niveau de log debug. Faux par défaut */
bool debug_logger=false;

/** \~french Message d'usage de la commande merge4tiff */
std::string help = std::string("\nmerge4tiff version ") + std::string(VERSION) + "\n\n"

    "Four images subsampling, formed a square, might use a background and data masks\n\n"

    "Usage: merge4tiff [-g <VAL>] -n <VAL> [-c <VAL>] [-iX <FILE> [-mX<FILE>]] -io <FILE> [-mo <FILE>]\n\n"

    "Parameters:\n"
    "     -g gamma float value, to dark (0 < g < 1) or brighten (1 < g) 8-bit integer images' subsampling\n"
    "     -n nodata value, one interger per sample, seperated with comma. Examples\n"
    "             -99999 for DTM\n"
    "             255,255,255 for orthophotography\n"
    "     -c output compression :\n"
    "             raw     no compression\n"
    "             none    no compression\n"
    "             jpg     Jpeg encoding (quality 75)\n"
    "             jpg90   Jpeg encoding (quality 90)\n"
    "             lzw     Lempel-Ziv & Welch encoding\n"
    "             pkb     PackBits encoding\n"
    "             zip     Deflate encoding\n\n"

    "     -io output image\n"
    "     -mo output mask (optionnal)\n\n"

    "     -iX input images\n"
    "             X = [1..4]      give input image position\n"
    "                     image1 | image2\n"
    "                     -------+-------\n"
    "                     image3 | image4\n\n"

    "             X = b           background image\n"
    "     -mX input associated masks (optionnal)\n"
    "             X = [1..4] or X = b\n"
    "     -a sample format : (float32 or uint8)\n"
    "     -s samples per pixel : (1, 2, 3 or 4)\n"
    "     -d debug logger activation\n\n"

    "If sampleformat or samplesperpixel are not provided, those informations are read from the image sources (all have to own the same). If all are provided, conversion may be done.\n\n"

    "Examples\n"
    "     - without mask, with background image\n"
    "     merge4tiff -g 1 -n 255,255,255 -c zip -ib background_image_path.tif -i1 image1.tif -i3 image3.tif -io imageOut.tif\n\n"

    "     - with mask, without background image\n"
    "     merge4tiff -g 1 -n 255,255,255 -c zip -i1 image1.tif -m1 mask1.tif -i3 image3.tif -m3 mask3.tif -mo maskOut.tif  -io imageOut.tif\n";

/**
 * \~french
 * \brief Affiche l'utilisation et les différentes options de la commande merge4tiff # help
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
 * \~french
 * \brief Récupère les valeurs passées en paramètres de la commande, et les stocke dans les variables globales
 * \param[in] argc nombre de paramètres
 * \param[in] argv tableau des paramètres
 * \return code de retour, 0 si réussi, -1 sinon
 */
int parse_command_line ( int argc, char* argv[] ) {

    for ( int i = 1; i < argc; i++ ) {
        if ( argv[i][0] == '-' ) {
            switch ( argv[i][1] ) {
            case 'h': // help
                usage();
                exit ( 0 );
            case 'd': // debug logs
                debug_logger = true;
                break;
            case 'g': // gamma
                if ( ++i == argc ) {
                    BOOST_LOG_TRIVIAL(error) <<  "Error in option -g" ;
                    return -1;
                }
                local_gamma = atof ( argv[i] );
                if ( local_gamma <= 0. ) {
                    BOOST_LOG_TRIVIAL(error) <<  "Unvalid parameter in -g argument, have to be positive" ;
                    return -1;
                }
                break;
            case 'n': // nodata
                if ( ++i == argc ) {
                    BOOST_LOG_TRIVIAL(error) <<  "Error in option -n" ;
                    return -1;
                }
                strnodata = argv[i];
                break;
            case 'c': // compression
                if ( ++i == argc ) {
                    BOOST_LOG_TRIVIAL(error) <<  "Error in option -c" ;
                    return -1;
                }
                if ( strncmp ( argv[i], "none",4 ) == 0 ) compression = Compression::NONE;
                else if ( strncmp ( argv[i], "raw",3 ) == 0 ) compression = Compression::NONE;
                else if ( strncmp ( argv[i], "zip",3 ) == 0 ) compression = Compression::DEFLATE;
                else if ( strncmp ( argv[i], "pkb",3 ) == 0 ) compression = Compression::PACKBITS;
                else if ( strncmp ( argv[i], "jpg90",5 ) == 0 ) compression = Compression::JPEG90;
                else if ( strncmp ( argv[i], "jpg",3 ) == 0 ) compression = Compression::JPEG;
                else if ( strncmp ( argv[i], "lzw",3 ) == 0 ) compression = Compression::LZW;
                else {
                    BOOST_LOG_TRIVIAL(error) <<  "Unknown value for option -c : " << argv[i] ;
                    return -1;
                }
                break;

            case 'i': // images
                if ( ++i == argc ) {
                    BOOST_LOG_TRIVIAL(error) <<  "Error in option -i" ;
                    return -1;
                }
                switch ( argv[i-1][2] ) {
                case '1':
                    input_images_paths[0] = std::string(argv[i]);
                    break;
                case '2':
                    input_images_paths[1] = std::string(argv[i]);
                    break;
                case '3':
                    input_images_paths[2] = std::string(argv[i]);
                    break;
                case '4':
                    input_images_paths[3] = std::string(argv[i]);
                    break;
                case 'b':
                    background_image_path = std::string(argv[i]);
                    break;
                case 'o':
                    output_image_path = std::string(argv[i]);
                    break;
                default:
                    BOOST_LOG_TRIVIAL(error) <<  "Unknown image's indice : -m" << argv[i-1][2] ;
                    return -1;
                }
                break;
            case 'm': // associated masks
                if ( ++i == argc ) {
                    BOOST_LOG_TRIVIAL(error) <<  "Error in option -m" ;
                    return -1;
                }
                switch ( argv[i-1][2] ) {
                case '1':
                    input_masks_paths[0] = std::string(argv[i]);
                    break;
                case '2':
                    input_masks_paths[1] = std::string(argv[i]);
                    break;
                case '3':
                    input_masks_paths[2] = std::string(argv[i]);
                    break;
                case '4':
                    input_masks_paths[3] = std::string(argv[i]);
                    break;
                case 'b':
                    background_mask_path = std::string(argv[i]);
                    break;
                case 'o':
                    output_mask_path = std::string(argv[i]);
                    break;
                default:
                    BOOST_LOG_TRIVIAL(error) <<  "Unknown mask's indice : -m" << argv[i-1][2] ;
                    return -1;
                }
                break;

            /****************** OPTIONNEL, POUR FORCER DES CONVERSIONS **********************/
            case 's': // samplesperpixel
                if ( i++ >= argc ) {
                    BOOST_LOG_TRIVIAL(error) <<  "Error in option -s" ;
                    return -1;
                }
                if ( strncmp ( argv[i], "1",1 ) == 0 ) samplesperpixel = 1 ;
                else if ( strncmp ( argv[i], "2",1 ) == 0 ) samplesperpixel = 2 ;
                else if ( strncmp ( argv[i], "3",1 ) == 0 ) samplesperpixel = 3 ;
                else if ( strncmp ( argv[i], "4",1 ) == 0 ) samplesperpixel = 4 ;
                else {
                    BOOST_LOG_TRIVIAL(error) <<  "Unknown value for option -s : " << argv[i] ;
                    return -1;
                }
                break;
            case 'a': // sampleformat
                if ( i++ >= argc ) {
                    BOOST_LOG_TRIVIAL(error) <<  "Error in option -a" ;
                    return -1;
                }
                if ( strncmp ( argv[i],"uint8",5 ) == 0 ) sampleformat = SampleFormat::UINT8 ;
                else if ( strncmp ( argv[i],"float32",7 ) == 0 ) sampleformat = SampleFormat::FLOAT32;
                else {
                    BOOST_LOG_TRIVIAL(error) <<  "Unknown value for option -a : " << argv[i] ;
                    return -1;
                }
                break;
            /*******************************************************************************/

            default:
                BOOST_LOG_TRIVIAL(error) <<  "Unknown option : -" << argv[i][1] ;
                return -1;
            }
        }
    }

    /* Obligatoire :
     *  - la valeur de nodata
     *  - l'image de sortie
     */
    if ( strnodata == 0 ) {
        BOOST_LOG_TRIVIAL(error) <<  "Missing nodata value" ;
        return -1;
    }
    if ( output_image_path.empty() ) {
        BOOST_LOG_TRIVIAL(error) <<  "Missing output file" ;
        return -1;
    }

    return 0;
}

/**
 * \~french
 * \brief Contrôle les caractéristiques d'une image (format des canaux, tailles) et de son éventuel masque.
 * \details Si les composantes sont bonnes, le masque est attaché à l'image.
 * \param[in] image image à contrôler
 * \param[in] position position de l'image (0, 1, 2 ou 3)
 * \return code de retour, 0 si réussi, -1 sinon
 */
int check_components ( FileImage* image, int position ) {

    if ( width == 0 ) { // read the parameters of the first input file
        width = image->get_width();
        height = image->get_height();
        
        if ( width%2 || height%2 ) {
            BOOST_LOG_TRIVIAL(error) <<  "Sorry : only even dimensions for input images are supported" ;
            return -1;
        }

        if (! output_format_provided) {
            // On n'a pas précisé de format de sortie
            // Toutes les images en entrée doivent avoir le même format
            // La sortie aura ce format
            photometric = image->get_photometric();
            sampleformat = image->get_sample_format();
            samplesperpixel = image->get_channels();
        } else {
            // La photométrie est déduite du nombre de canaux
            if (samplesperpixel == 1) {
                photometric = Photometric::GRAY;
            } else if (samplesperpixel == 2) {
                photometric = Photometric::GRAY;
            } else {
                photometric = Photometric::RGB;
            }
        }

        if ( sampleformat == SampleFormat::UNKNOWN ) {
            BOOST_LOG_TRIVIAL(error) <<  "Unknown sample format" ;
            return -1;
        }
    } else {

        if ( image->get_width() != width || image->get_height() != height) {
            BOOST_LOG_TRIVIAL(error) <<  "Error : all input image must have the same dimensions (width, height) : " << image->get_filename();
            return -1;
        }

        if (! output_format_provided) {
            if (image->get_sample_format() != sampleformat || image->get_photometric() != photometric || image->get_channels() != samplesperpixel) {
                BOOST_LOG_TRIVIAL(error) << "Error : output format is not provided, so all input image must have the same format (sample format, channels, etc...) : " <<  image->get_filename();
                return -1;
            }
        }
    }

    // On va ajouter une bbox pour que le positionnement dans l'ExtendedCompoundImage soit correct
    switch (position)
    {
    case -1:
        image->set_bbox(BoundingBox<double> (0., 0., width, height));
        break;
    case 0:
        image->set_bbox(BoundingBox<double> (0., height / 2., width / 2., height));
        break;
    case 1:
        image->set_bbox(BoundingBox<double> (width / 2., height / 2., width, height));
        break;
    case 2:
        image->set_bbox(BoundingBox<double> (0., 0., width / 2., height / 2.));
        break;
    case 3:
        image->set_bbox(BoundingBox<double> (width / 2., 0., width, height / 2.));
        break;
    }

    if (output_format_provided) {
        bool ok = image->add_converter ( sampleformat, samplesperpixel );
        if (! ok ) {
            BOOST_LOG_TRIVIAL(error) <<  "Cannot add converter to the input FileImage " << image->get_filename() ;
            return -1;
        }
    }

    return 0;
}

/**
 * \~french
 * \brief Contrôle l'ensemble des images et masques, en entrée et sortie
 * \details Crée les objets TIFF, contrôle la cohérence des caractéristiques des images en entrée, ouvre les flux de lecture et écriture. Les éventuels masques associés sont ajoutés aux objets FileImage.
 * \param[in] output_image image en sortie
 * \param[in] output_mask masque en sortie
 * \param[in] input_images image composée en entrée
 * \return code de retour, 0 si réussi, -1 sinon
 */
int load_images ( FileImage** output_image, FileImage** output_mask, Image **input_image) {
    width = 0;

    std::vector<Image *> input_images;

    bool missing_image = false;
    bool with_mask = false;

    BOOST_LOG_TRIVIAL(debug) <<  "Pack input images" ;

    for ( int i = 0; i < 4; i++ ) {
        BOOST_LOG_TRIVIAL(debug) <<  "Place " << i ;
        // Initialisation
        if ( input_images_paths[i].empty() ) {
            BOOST_LOG_TRIVIAL(debug) <<  "No image" ;
            missing_image = true;
            continue;
        }

        // Image en entrée
        FileImage* inputi = FileImage::create_to_read(input_images_paths[i]);
        if ( inputi == NULL ) {
            BOOST_LOG_TRIVIAL(error) <<  "Unable to open input image: " << input_images_paths[i] ;
            return -1;
        }

        // Eventuel masque associé
        if ( ! input_masks_paths[i].empty() ) {
            FileImage* inputm = FileImage::create_to_read(input_masks_paths[i]);
            if ( inputm == NULL ) {
                BOOST_LOG_TRIVIAL(error) <<  "Unable to open input mask: " << input_masks_paths[i] ;
                return -1;
            }
            if (! inputi->set_mask(inputm)) {
                BOOST_LOG_TRIVIAL(error) <<  "Unable to associate mask to image input mask: " << input_masks_paths[i] ;
                return -1;
            }

            with_mask = true;
        }

        // Controle des composantes des images/masques et association
        BOOST_LOG_TRIVIAL(debug) <<  "Check" ;
        if ( check_components ( inputi, i ) < 0 ) {
            BOOST_LOG_TRIVIAL(error) <<  "Unvalid components for the image " << input_images_paths[i];
            return -1;
        }

        input_images.push_back(inputi);
    }

    BOOST_LOG_TRIVIAL(debug) <<  "Nodata interpretation" ;
    // Conversion string->int[] du paramètre nodata
    int int_nodata[samplesperpixel];

    std::vector<std::string> vector_nodata;
    boost::split(vector_nodata, strnodata, boost::is_any_of(","));
    if (vector_nodata.size() != samplesperpixel) {
        BOOST_LOG_TRIVIAL(error) << "Error with option -n : a value for nodata is missing (" << vector_nodata.size() << " value(s) provided for " << samplesperpixel << " band output)";
        BOOST_LOG_TRIVIAL(error) << output_format_provided;
        return -1;
    }
    
    for ( int i = 0; i < samplesperpixel; i++ ) {
        int_nodata[i] = atoi ( vector_nodata.at(i).c_str() );
    }

    ExtendedCompoundImage* eci = ExtendedCompoundImage::create(input_images, int_nodata, 0);
    if ( eci == NULL ) {
        BOOST_LOG_TRIVIAL(error) <<  "Unable to pack input images";
        return -1;
    }
    ExtendedCompoundMask* ecm = new ExtendedCompoundMask(eci);

    if (! eci->set_mask(ecm)) {
        BOOST_LOG_TRIVIAL(error) << "Cannot add mask to the Image's pack";
        return -1;
    }

    SubsampledImage* si = SubsampledImage::create(eci, 2, 2);
    if ( si == NULL ) {
        BOOST_LOG_TRIVIAL(error) <<  "Unable to subsample input images";
        return -1;
    }

    SubsampledImage* sm = SubsampledImage::create(ecm, 2, 2);
    if ( si == NULL ) {
        BOOST_LOG_TRIVIAL(error) <<  "Unable to subsample input masks";
        return -1;
    }

    if (! si->set_mask(sm)) {
        BOOST_LOG_TRIVIAL(error) << "Cannot add mask to the Subsmapled image";
        return -1;
    }

    *input_image = si;

    // Si il manque une image ou qu'on a au moins un masque, on doit prendre en compte le fond
    if ( (missing_image || with_mask) && ! background_image_path.empty() ) {
        BOOST_LOG_TRIVIAL(debug) <<  "Add background" ;

        FileImage* bgi = FileImage::create_to_read(background_image_path);
        if ( bgi == NULL ) {
            BOOST_LOG_TRIVIAL(error) <<  "Unable to open background image: " << background_image_path ;
            return -1;
        }

        if ( ! background_mask_path.empty() ) {
            FileImage* bgm = FileImage::create_to_read(background_mask_path);
            if ( bgm == NULL ) {
                BOOST_LOG_TRIVIAL(error) <<  "Unable to open background mask: " << background_mask_path ;
                return -1;
            }
            if (! bgi->set_mask(bgm)) {
                BOOST_LOG_TRIVIAL(error) <<  "Unable to associate background mask to background image: " << background_mask_path ;
                return -1;
            }

        }

        // Controle des composantes des images/masques
        BOOST_LOG_TRIVIAL(debug) <<  "Check" ;
        if ( check_components ( bgi, -1 ) < 0 ) {
            BOOST_LOG_TRIVIAL(error) <<  "Unvalid components for the background image " << background_image_path << " (or its mask)" ;
            return -1;
        }

        std::vector<Image*> with_bg = {bgi, si};
        MergeImage* mi = MergeImage::create(with_bg, samplesperpixel, int_nodata, NULL, Merge::NORMAL );

        MergeMask* merged_mask = new MergeMask ( mi );

        if (! mi->set_mask(merged_mask)) {
            BOOST_LOG_TRIVIAL(error) << "Cannot add mask to the Image's pack with background";
            return -1;
        }

        *input_image = mi;
    }

    /********************** EN SORTIE ***********************/

    *output_image = NULL;
    *output_mask = NULL;

    *output_image = FileImage::create_to_write(output_image_path, BoundingBox<double>(0,0,0,0), -1, -1, width, height,
                                     samplesperpixel, sampleformat, photometric, compression);
    if ( output_image == NULL ) {
        BOOST_LOG_TRIVIAL(error) <<  "Unable to open output image: " << output_image_path ;
        return -1;
    }

    if ( ! output_mask_path.empty() ) {
        *output_mask = FileImage::create_to_write(output_mask_path, BoundingBox<double>(0,0,0,0), -1, -1, width, height,
                                                   1, SampleFormat::UINT8, Photometric::MASK, Compression::DEFLATE);
        if ( *output_mask == NULL ) {
            BOOST_LOG_TRIVIAL(error) <<  "Unable to open output mask: " << output_mask_path ;
            return -1;
        }
    }

    return 0;
}

// uint8_t merge_weights[1024];
// for ( int i = 0; i <= 1020; i++ ) merge_weights[i] = 255 - ( uint8_t ) round ( pow ( double ( 1020 - i ) /1020., local_gamma ) * 255. );


/**
 ** \~french
 * \brief Fonction principale de l'outil merge4tiff
 * \details Différencie le cas de canaux flottants sur 32 bits des canaux entier non signés sur 8 bits.
 * \param[in] argc nombre de paramètres
 * \param[in] argv tableau des paramètres
 * \return 0 en cas de succès, -1 sinon
 ** \~english
 * \brief Main function for tool merge4tiff
 * \param[in] argc parameters number
 * \param[in] argv parameters array
 * \return 0 if success, -1 otherwise
 */
int main ( int argc, char* argv[] ) {
    Image* input_image = NULL;
    FileImage* output_image = NULL;
    FileImage* output_mask = NULL;

    /* Initialisation des Loggers */
    boost::log::core::get()->set_filter( boost::log::trivial::severity >= boost::log::trivial::info );
    logging::add_common_attributes();
    boost::log::register_simple_formatter_factory< boost::log::trivial::severity_level, char >("Severity");
    logging::add_console_log (
        std::cout,
        keywords::format = "%Severity%\t%Message%"
    );

    BOOST_LOG_TRIVIAL(debug) <<  "Parse" ;
    // Lecture des parametres de la ligne de commande
    if ( parse_command_line ( argc, argv ) < 0 ) {
        error ( "Echec lecture ligne de commande",-1 );
    }

    // On sait maintenant si on doit activer le niveau de log DEBUG
    if (debug_logger) {
        boost::log::core::get()->set_filter( boost::log::trivial::severity >= boost::log::trivial::debug );
    }

    // On regarde si on a tout précisé en sortie, pour voir si des conversions sont possibles
    if (sampleformat != SampleFormat::UNKNOWN && samplesperpixel != 0) {
        output_format_provided = true;
    }

    BOOST_LOG_TRIVIAL(debug) <<  "Load images" ;
    // Chargement des images
    if ( load_images ( &output_image, &output_mask, &input_image ) < 0 ) {
        if (input_image) delete input_image;
        if (output_image) delete output_image;
        if (output_mask) delete output_mask;

        error ( "Echec chargement des images",-1 );
    }

    BOOST_LOG_TRIVIAL(debug) << "Save image";
    // Enregistrement de l'image fusionnée
    if (output_image->write_image(input_image) < 0) {
        if (input_image) delete input_image;
        if (output_image) delete output_image;
        if (output_mask) delete output_mask;
        CrsBook::clean_crss();
        ProjPool::clean_projs();
        proj_cleanup();
        error("Echec enregistrement de l image finale", -1);
    }

    if (output_mask != NULL) {
        BOOST_LOG_TRIVIAL(debug) << "Save mask";
        // Enregistrement du masque fusionné, si demandé
        if (output_mask->write_image(input_image->Image::get_mask()) < 0) {
            if (input_image) delete input_image;
            if (output_image) delete output_image;
            if (output_mask) delete output_mask;
            CrsBook::clean_crss();
            ProjPool::clean_projs();
            proj_cleanup();
            error("Echec enregistrement du masque final", -1);
        }
    }


    BOOST_LOG_TRIVIAL(debug) <<  "Clean" ;
    
    if (input_image) delete input_image;
    if (output_image) delete output_image;
    if (output_mask) delete output_mask;

    CrsBook::clean_crss();
    ProjPool::clean_projs();
    proj_cleanup();
}

