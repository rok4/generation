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
 * \file merge4tiff.cpp
 * \author Institut national de l'information géographique et forestière
 * \~french \brief Sous echantillonage de 4 images disposées en carré, avec utilisation possible de fond et de masques de données
 * \~english \brief Four images subsampling, formed a square, might use a background and data masks
 * \~ \image html merge4tiff.png
 */

#include <tiffio.h>
#include <rok4/image/Image.h>
#include <rok4/enums/Format.h>
#include <rok4/utils/Cache.h>
#include <rok4/image/file/FileImage.h>

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
#include "config.h"

/* Valeurs de nodata */
/** \~french Valeur de nodata sour forme de chaîne de caractère (passée en paramètre de la commande) */
char* strnodata;

/* Chemins des images en entrée et en sortie */
/** \~french Chemin de l'image de fond */
char* background_image_path;
/** \~french Chemin du masque associé à l'image de fond */
char* background_mask_path;
/** \~french Chemins des images en entrée */
char* input_images_paths[4];
/** \~french Chemins des masques associés aux images en entrée */
char* input_masks_paths[4];
/** \~french Chemin de l'image en sortie */
char* output_image_path;
/** \~french Chemin du masque associé à l'image en sortie */
char* output_mask_path;

/* Caractéristiques des images en entrée et en sortie */
/** \~french Valeur de gamma, pour foncer ou éclaircir des images en entier */
double local_gamma;
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
std::string help = std::string("\ncache2work version ") + std::string(VERSION) + "\n\n"

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
    "     -a sample format : (float or uint)\n"
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
    // Initialisation
    local_gamma = 1.;
    strnodata = 0;
    compression = Compression::NONE;
    background_image_path = 0;
    background_mask_path = 0;
    for ( int i=0; i<4; i++ ) {
        input_images_paths[i] = 0;
        input_masks_paths[i] = 0;
    }
    output_image_path = 0;
    output_mask_path = 0;

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
                    input_images_paths[0] = argv[i];
                    break;
                case '2':
                    input_images_paths[1] = argv[i];
                    break;
                case '3':
                    input_images_paths[2] = argv[i];
                    break;
                case '4':
                    input_images_paths[3] = argv[i];
                    break;
                case 'b':
                    background_image_path = argv[i];
                    break;
                case 'o':
                    output_image_path = argv[i];
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
                    input_masks_paths[0] = argv[i];
                    break;
                case '2':
                    input_masks_paths[1] = argv[i];
                    break;
                case '3':
                    input_masks_paths[2] = argv[i];
                    break;
                case '4':
                    input_masks_paths[3] = argv[i];
                    break;
                case 'b':
                    background_mask_path = argv[i];
                    break;
                case 'o':
                    output_mask_path = argv[i];
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
    if ( output_image_path == 0 ) {
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
 * \param[in] mask précise éventuellement un masque de donnée
 * \return code de retour, 0 si réussi, -1 sinon
 */
int check_components ( FileImage* image, FileImage* mask) {

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

    if (mask != NULL) {
        if ( ! ( mask->get_width() == width && mask->get_height() == height && mask->get_sample_format() == SampleFormat::UINT8 && 
                mask->get_photometric() == Photometric::GRAY && mask->get_channels() == 1 ) ) {

            BOOST_LOG_TRIVIAL(error) <<  "Error : all input masks must have the same parameters (width, height, etc...) : " << mask->get_filename();
            return -1;
        }

        if ( ! image->set_mask(mask) ) {
            BOOST_LOG_TRIVIAL(error) <<  "Cannot add associated mask to the input FileImage " << image->get_filename() ;
            return -1;
        }
        
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
 * \param[in] input_images images en entrée
 * \param[in] background_image image de fond en entrée
 * \param[in] output_image image en sortie
 * \return code de retour, 0 si réussi, -1 sinon
 */
int check_images ( FileImage* input_images[2][2], FileImage*& background_image, FileImage*& output_image, FileImage*& output_mask) {
    width = 0;

    for ( int i = 0; i < 4; i++ ) {
        BOOST_LOG_TRIVIAL(debug) <<  "Place " << i ;
        // Initialisation
        if ( input_images_paths[i] == 0 ) {
            BOOST_LOG_TRIVIAL(debug) <<  "No image" ;
            input_images[i/2][i%2] = NULL;
            continue;
        }

        // Image en entrée
        FileImage* inputi = FileImage::create_to_read(input_images_paths[i]);
        if ( inputi == NULL ) {
            BOOST_LOG_TRIVIAL(error) <<  "Unable to open input image: " + std::string ( input_images_paths[i] ) ;
            return -1;
        }
        input_images[i/2][i%2] = inputi;

        // Eventuelle masque associé
        FileImage* input_mask = NULL;
        if ( input_masks_paths[i] != 0 ) {
            input_mask = FileImage::create_to_read(input_masks_paths[i]);
            if ( input_mask == NULL ) {
                BOOST_LOG_TRIVIAL(error) <<  "Unable to open input mask: " << std::string ( input_masks_paths[i] ) ;
                return -1;
            }
        }

        // Controle des composantes des images/masques et association
        BOOST_LOG_TRIVIAL(debug) <<  "Check" ;
        if ( check_components ( inputi, input_mask ) < 0 ) {
            BOOST_LOG_TRIVIAL(error) <<  "Unvalid components for the image " << std::string ( input_images_paths[i] ) << " (or its mask)" ;
            return -1;
        }  
    }

    background_image = NULL;

    // Si on a quatre image et pas de masque (images considérées comme pleines), le fond est inutile
    if ( input_images_paths[0] && input_images_paths[1] && input_images_paths[2] && input_images_paths[3] &&
            ! input_masks_paths[0] && ! input_masks_paths[1] && ! input_masks_paths[2] && ! input_masks_paths[3] )
        
        background_image_path=0;

    if ( background_image_path ) {
        background_image = FileImage::create_to_read(background_image_path);
        if ( background_image == NULL ) {
            BOOST_LOG_TRIVIAL(error) <<  "Unable to open background image: " + std::string ( background_image_path ) ;
            return -1;
        }

        FileImage* background_mask = NULL;

        if ( background_mask_path ) {
            background_mask = FileImage::create_to_read(background_mask_path);
            if ( background_mask == NULL ) {
                BOOST_LOG_TRIVIAL(error) <<  "Unable to open background mask: " + std::string ( background_mask_path ) ;
                return -1;
            }
        }

        // Controle des composantes des images/masques
        if ( check_components ( background_image, background_mask ) < 0 ) {
            BOOST_LOG_TRIVIAL(error) <<  "Unvalid components for the background image " << std::string ( background_image_path ) << " (or its mask)" ;
            return -1;
        }
    }

    /********************** EN SORTIE ***********************/

    output_image = NULL;
    output_mask = NULL;

    output_image = FileImage::create_to_write(output_image_path, BoundingBox<double>(0,0,0,0), -1, -1, width, height,
                                     samplesperpixel, sampleformat, photometric, compression);
    if ( output_image == NULL ) {
        BOOST_LOG_TRIVIAL(error) <<  "Unable to open output image: " + std::string ( output_image_path ) ;
        return -1;
    }

    if ( output_mask_path ) {
        output_mask = FileImage::create_to_write(output_mask_path, BoundingBox<double>(0,0,0,0), -1, -1, width, height,
                                                   1, SampleFormat::UINT8, Photometric::MASK, Compression::DEFLATE);
        if ( output_mask == NULL ) {
            BOOST_LOG_TRIVIAL(error) <<  "Unable to open output mask: " + std::string ( output_mask_path ) ;
            return -1;
        }

        output_image->set_mask(output_mask);
    }

    return 0;
}


/**
 * \~french
 * \brief Remplit un buffer à partir d'une ligne d'une image et d'un potentiel masque associé
 * \details les pixels qui ne contiennent pas de donnée sont remplis avec la valeur de nodata
 * \param[in] background_image image de fond à lire
 * \param[out] image_line ligne de l'image en sortie
 * \param[out] mask_line ligne du masque en sortie
 * \param[in] line indice de la ligne source dans l'image (et son masque)
 * \param[in] nodata valeur de nodata
 * \return code de retour, 0 si réussi, -1 sinon
 */
template <typename T>
int fill_background_line ( FileImage* background_image, T* image_line, uint8_t* mask_line, int line, T* nodata ) {
    if ( background_image->get_line( image_line, line ) == 0 ) return 1;

    if ( background_image->get_mask() != NULL ) {
        if ( background_image->get_mask()->get_line( mask_line, line ) == 0 ) return 1;
        for ( int w = 0; w < width; w++ ) {
            if ( mask_line[w] == 0 ) {
                memcpy ( image_line + w*samplesperpixel, nodata,samplesperpixel*sizeof ( T ) );
            }
        }
    } else {
        memset ( mask_line,255,width );
    }

    return 0;
}

/**
 * \~french
 * \brief Fusionne les 4 images en entrée et le masque de fond dans l'image de sortie
 * \details Dans le cas entier, lors de la moyenne des 4 pixels, on utilise une valeur de gamma qui éclaircit (si supérieure à 1.0) ou fonce (si inférieure à 1.0) le résultat. Si gamma vaut 1, le résultat est une moyenne classique. Les masques sont déjà associé aux objets FileImage, sauf pour l'image de sortie.
 * \param[in] background_image image de fond en entrée
 * \param[in] input_images images en entrée
 * \param[in] output_image image en sortie
 * \param[in] output_image éventuel masque en sortie
 * \return code de retour, 0 si réussi, -1 sinon
 */
template <typename T>
int merge ( FileImage* background_image, FileImage* input_images[2][2], FileImage* output_image, FileImage* output_mask, T* nodata ) {
    
    uint8 merge_weights[1024];
    for ( int i = 0; i <= 1020; i++ ) merge_weights[i] = 255 - ( uint8 ) round ( pow ( double ( 1020 - i ) /1020., local_gamma ) * 255. );

    int samples_count = width * samplesperpixel;
    int left,right;

    T background_image_line[samples_count];
    uint8_t background_mask_line[width];

    int data_count;
    float pixel[samplesperpixel];

    T input_images_line_1[2*samples_count];
    uint8_t input_masks_line_1[2*width];

    T input_images_line_2[2*samples_count];
    uint8_t input_masks_line_2[2*width];

    T output_image_line[samples_count];
    uint8_t output_mask_line[width];

    // ----------- initialisation du fond -----------
    for ( int i = 0; i < samples_count ; i++ )
        background_image_line[i] = nodata[i%samplesperpixel];

    memset ( background_mask_line,0,width );

    for ( int y = 0; y < 2; y++ ) {
        if ( input_images[y][0] ) left = 0;
        else left = width;
        if ( input_images[y][1] ) right = 2*width;
        else right = width;

        for ( uint32 h = 0; h < height / 2; h++ ) {

            int line = y * height / 2 + h;

            // ------------------- le fond ------------------
            if ( background_image )
                if ( fill_background_line ( background_image, background_image_line, background_mask_line, line, nodata ) ) {
                    BOOST_LOG_TRIVIAL(error) <<  "Unable to read background line" ;
                    return -1;
                }

            if ( left == right ) {
                // On n'a pas d'image en entrée pour cette ligne, on stocke le fond et on passe à la suivante
                if ( output_image->write_line( background_image_line, line ) == -1 ) {
                    BOOST_LOG_TRIVIAL(error) <<  "Unable to write image's line " << line ;
                    return -1;
                }
                if ( output_mask )
                    if ( output_mask->write_line( background_mask_line, line ) == -1 ) {
                        BOOST_LOG_TRIVIAL(error) <<  "Unable to write mask's line " << line ;
                        return -1;
                    }

                continue;
            }

            // -- initialisation de la sortie avec le fond --
            memcpy ( output_image_line,background_image_line,samples_count*sizeof ( T ) );
            memcpy ( output_mask_line,background_mask_line,width );
            
            memset ( input_masks_line_1,255,2*width );
            memset ( input_masks_line_2,255,2*width );

            // ----------------- les images -----------------
            // ------ et les éventuels masques --------------
            if ( input_images[y][0] ) {
                if ( input_images[y][0]->get_line( input_images_line_1, 2*h ) == 0 ) {
                    BOOST_LOG_TRIVIAL(error) <<  "Unable to read data line" ;
                    return -1;
                }
                if ( input_images[y][0]->get_line( input_images_line_2, 2*h+1 ) == 0 ) {
                    BOOST_LOG_TRIVIAL(error) <<  "Unable to read data line" ;
                    return -1;
                }

                if ( input_images[y][0]->get_mask() ) {
                    if ( input_images[y][0]->get_mask()->get_line( input_masks_line_1, 2*h ) == 0 ) {
                        BOOST_LOG_TRIVIAL(error) <<  "Unable to read data line" ;
                        return -1;
                    }
                    if ( input_images[y][0]->get_mask()->get_line( input_masks_line_2, 2*h+1 ) == 0 ) {
                        BOOST_LOG_TRIVIAL(error) <<  "Unable to read data line" ;
                        return -1;
                    }
                }
            }


            if ( input_images[y][1] ) {
                if ( input_images[y][1]->get_line( input_images_line_1 + samples_count, 2*h ) == 0 ) {
                    BOOST_LOG_TRIVIAL(error) <<  "Unable to read data line" ;
                    return -1;
                }
                if ( input_images[y][1]->get_line( input_images_line_2 + samples_count, 2*h+1 ) == 0 ) {
                    BOOST_LOG_TRIVIAL(error) <<  "Unable to read data line" ;
                    return -1;
                }

                if ( input_images[y][1]->get_mask() ) {
                    if ( input_images[y][1]->get_mask()->get_line( input_masks_line_1 + width, 2*h ) == 0 ) {
                        BOOST_LOG_TRIVIAL(error) <<  "Unable to read data line" ;
                        return -1;
                    }
                    if ( input_images[y][1]->get_mask()->get_line( input_masks_line_2 + width, 2*h+1 ) == 0 ) {
                        BOOST_LOG_TRIVIAL(error) <<  "Unable to read data line" ;
                        return -1;
                    }
                }
            }

            // ----------------- la moyenne ----------------
            for ( int input_pixel = left, input_sample = left * samplesperpixel; input_pixel < right;
                    input_pixel += 2, input_sample += 2*samplesperpixel ) {

                memset ( pixel,0,samplesperpixel*sizeof ( float ) );
                data_count = 0;

                if ( input_masks_line_1[input_pixel] ) {
                    data_count++;
                    for ( int c = 0; c < samplesperpixel; c++ ) pixel[c] += input_images_line_1[input_sample+c];
                }

                if ( input_masks_line_1[input_pixel+1] ) {
                    data_count++;
                    for ( int c = 0; c < samplesperpixel; c++ ) pixel[c] += input_images_line_1[input_sample+samplesperpixel+c];
                }

                if ( input_masks_line_2[input_pixel] ) {
                    data_count++;
                    for ( int c = 0; c < samplesperpixel; c++ ) pixel[c] += input_images_line_2[input_sample+c];
                }

                if ( input_masks_line_2[input_pixel+1] ) {
                    data_count++;
                    for ( int c = 0; c < samplesperpixel; c++ ) pixel[c] += input_images_line_2[input_sample+samplesperpixel+c];
                }

                if ( data_count > 1 ) {
                    output_mask_line[input_pixel/2] = 255;
                    if ( sizeof ( T ) == 1 ) {
                        // Cas entier : utilisation d'un gamma
                        for ( int c = 0; c < samplesperpixel; c++ ) output_image_line[input_sample/2+c] = merge_weights[ ( int ) pixel[c]*4/data_count];
                    } else if ( sizeof ( T ) == 4 ) {
                        for ( int c = 0; c < samplesperpixel; c++ ) output_image_line[input_sample/2+c] = pixel[c]/ ( float ) data_count;
                    }
                }
            }

            if ( output_image->write_line( output_image_line, line ) == -1 ) {
                BOOST_LOG_TRIVIAL(error) <<  "Unable to write image" ;
                return -1;
            }
            if ( output_mask ) {
                if ( output_mask->write_line( output_mask_line, line ) == -1 ) {
                    BOOST_LOG_TRIVIAL(error) <<  "Unable to write mask" ;
                    return -1;
                }
            }
        }
    }

    return 0;
}

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
    FileImage* input_images[2][2];
    for ( int i = 0; i < 2; i++ ) for ( int j = 0; j < 2; j++ ) input_images[i][j] = NULL;
    FileImage* background_image = NULL;
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

    BOOST_LOG_TRIVIAL(debug) <<  "Check images" ;
    // Controle des images
    if ( check_images ( input_images, background_image, output_image, output_mask ) < 0 ) {
        if ( background_image ) delete background_image;

        for ( int i = 0; i < 2; i++ ) for ( int j = 0; j < 2; j++ ) {
            if ( input_images[i][j] ) {
                delete input_images[i][j] ;
            }
        }
        error ( "Echec controle des images",-1 );
    }

    BOOST_LOG_TRIVIAL(debug) <<  "Nodata interpretation" ;
    // Conversion string->int[] du paramètre nodata
    int nodata[samplesperpixel];

    char* char_iterator = strtok ( strnodata,"," );
    if ( char_iterator == NULL ) {
        error ( "Error with option -n : a value for nodata is missing",-1 );
    }
    nodata[0] = atoi ( char_iterator );
    for ( int i = 1; i < samplesperpixel; i++ ) {
        char_iterator = strtok ( NULL, "," );
        if ( char_iterator == NULL ) {
            error ( "Error with option -n : a value for nodata is missing",-1 );
        }
        nodata[i] = atoi ( char_iterator );
    }

    // Cas MNT
    if ( sampleformat == SampleFormat::FLOAT32 ) {
        BOOST_LOG_TRIVIAL(debug) <<  "Merge images (float)" ;
        float nodata[samplesperpixel];
        for ( int i = 0; i < samplesperpixel; i++ ) nodata[i] = ( float ) nodata[i];
        if ( merge<float> ( background_image, input_images, output_image, output_mask, nodata ) < 0 ) error ( "Unable to merge float images",-1 );
    }
    // Cas images
    else if ( sampleformat == SampleFormat::UINT8 ) {
        BOOST_LOG_TRIVIAL(debug) <<  "Merge images (uint8_t)" ;
        uint8_t nodata[samplesperpixel];
        for ( int i = 0; i < samplesperpixel; i++ ) nodata[i] = ( uint8_t ) nodata[i];
        if ( merge ( background_image, input_images, output_image, output_mask, nodata ) < 0 ) error ( "Unable to merge integer images",-1 );
    } else {
        error ( "Unhandled sample's format",-1 );
    }


    BOOST_LOG_TRIVIAL(debug) <<  "Clean" ;
    
    ProjPool::clean_projs();
    proj_cleanup();

    if ( background_image ) delete background_image;

    for ( int i = 0; i < 2; i++ ) for ( int j = 0; j < 2; j++ ) {
        if ( input_images[i][j] ) delete input_images[i][j] ;
    }

    delete output_image;
}

