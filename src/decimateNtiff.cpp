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
 * \file decimateNtiff.cpp
 * \author Institut national de l'information géographique et forestière
 * \~french \brief Création d'une image TIFF géoréférencée à partir de n images sources géoréférencées
 * \~english \brief Create one georeferenced TIFF image from several georeferenced images
 * \~ \image html decimateNtiff.png \~french
 */

#include <proj.h>
#include <pthread.h>
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <algorithm>
#include <string>
#include <fstream>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
namespace logging = boost::log;
namespace keywords = boost::log::keywords;

#include <rok4/image/file/FileImage.h>
#include <rok4/image/DecimatedImage.h>
#include <rok4/image/ExtendedCompoundImage.h>
#include <rok4/utils/Cache.h>

#include <rok4/enums/Format.h>
#include <math.h>
#include "config.h"

#ifndef __max
#define __max(a, b)   ( ((a) > (b)) ? (a) : (b) )
#endif
#ifndef __min
#define __min(a, b)   ( ((a) < (b)) ? (a) : (b) )
#endif

// Paramètres de la ligne de commande déclarés en global
/** \~french Chemin du fichier de configuration des images */
char configuration_path[256];
/** \~french Valeur de nodata sour forme de chaîne de caractère (passée en paramètre de la commande) */
char strnodata[256];

/** \~french A-t-on précisé le format en sortie, c'est à dire les 3 informations samplesperpixel et sample_format */
bool output_format_provided = false;
/** \~french Nombre de canaux par pixel, pour l'image en sortie */
uint16_t samplesperpixel = 0;
/** \~french Format du canal (entier, flottant, signé ou non...), pour l'image en sortie */
SampleFormat::eSampleFormat sample_format = SampleFormat::UNKNOWN;

/** \~french Photométrie (rgb, gray), déduit du nombre de canaux */
Photometric::ePhotometric photometric;

/** \~french Compression de l'image de sortie */
Compression::eCompression compression;

/** \~french Activation du niveau de log debug. Faux par défaut */
bool debug_logger=false;


/** \~french Message d'usage de la commande decimateNtiff */
std::string help = std::string("\ndecimateNtiff version ") + std::string(VERSION) + "\n\n"

    "Create one georeferenced TIFF image from several georeferenced TIFF images.\n\n"

    "Usage: decimateNtiff -f <FILE> -c <VAL> -n <VAL> [-d] [-h]\n"

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
    "    -n nodata value, one integer per sample, seperated with comma. Examples\n"
    "            -99999 for DTM\n"
    "            255,255,255 for orthophotography\n"
    "    -a sample format : (float32 or uint8)\n"
    "    -s samples per pixel : (1, 2, 3 or 4)\n"
    "    -d debug logger activation\n\n"

    "If sample_format or samplesperpixel are not provided, those informations are read from the image sources (all have to own the same). If all are provided, conversion may be done.\n\n";

/**
 * \~french
 * \brief Affiche l'utilisation et les différentes options de la commande decimateNtiff #help
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

    for ( int i = 1; i < argc; i++ ) {
        if ( argv[i][0] == '-' ) {
            switch ( argv[i][1] ) {
            case 'h': // help
                usage();
                exit ( 0 );
            case 'd': // debug logs
                debug_logger = true;
                break;
            case 'f': // fichier de liste des images source
                if ( i++ >= argc ) {
                    BOOST_LOG_TRIVIAL(error) <<  "Error in option -f" ;
                    return -1;
                }
                strcpy ( configuration_path,argv[i] );
                break;
            case 'n': // nodata
                if ( i++ >= argc ) {
                    BOOST_LOG_TRIVIAL(error) <<  "Error in option -n" ;
                    return -1;
                }
                strcpy ( strnodata,argv[i] );
                break;
            case 'c': // compression
                if ( i++ >= argc ) {
                    BOOST_LOG_TRIVIAL(error) <<  "Error in option -c" ;
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
                    BOOST_LOG_TRIVIAL(error) <<  "Unknown value for option -c : " << argv[i] ;
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
            case 'a': // sample_format
                if ( i++ >= argc ) {
                    BOOST_LOG_TRIVIAL(error) <<  "Error in option -a" ;
                    return -1;
                }
                if ( strncmp ( argv[i],"uint8",5 ) == 0 ) sample_format = SampleFormat::UINT8 ;
                else if ( strncmp ( argv[i],"float32",7 ) == 0 ) sample_format = SampleFormat::FLOAT32;
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

    BOOST_LOG_TRIVIAL(debug) <<  "decimateNtiff -f " << configuration_path ;

    return 0;
}

/**
 * \~french
 * \brief Lit l'ensemble de la configuration
 *
 * \param[in,out] masks Indicateurs de présence d'un masque
 * \param[in,out] paths Chemins des images
 * \param[in,out] bboxes Rectangles englobant des images
 * \param[in,out] resxs Résolution en x des images
 * \param[in,out] resys Résolution en y des images
 * \return true en cas de succès, false si échec
 */
bool load_configuration ( 
    std::vector<bool>* masks, 
    std::vector<char* >* paths, 
    std::vector<BoundingBox<double> >* bboxes,
    std::vector<double>* resxs,
    std::vector<double>* resys
) {

    std::ifstream file;

    file.open ( configuration_path );
    if ( ! file.is_open() ) {
        BOOST_LOG_TRIVIAL(error) <<  "Impossible d'ouvrir le fichier " << configuration_path ;
        return false;
    }

    while ( file.good() ) {
        char tmpPath[IMAGE_MAX_FILENAME_LENGTH];
        char tmpCRS[20];
        char line[2*IMAGE_MAX_FILENAME_LENGTH];
        memset ( line, 0, 2*IMAGE_MAX_FILENAME_LENGTH );
        memset ( tmpPath, 0, IMAGE_MAX_FILENAME_LENGTH );
        memset ( tmpCRS, 0, 20 );

        char type[3];
        BoundingBox<double> bb(0.,0.,0.,0.);
        double resx, resy;
        bool is_mask;

        file.getline(line, 2*IMAGE_MAX_FILENAME_LENGTH);
        BOOST_LOG_TRIVIAL(debug) << line;  
        if ( strlen(line) == 0 ) {
            continue;
        }
        int nb = std::sscanf ( line,"%s %s %lf %lf %lf %lf %lf %lf", type, tmpPath, &bb.xmin, &bb.ymax, &bb.xmax, &bb.ymin, &resx, &resy );
        if ( nb == 8 && memcmp ( type,"IMG",3 ) == 0) {
            // On lit la ligne d'une image
            is_mask = false;
        }
        else if ( nb == 2 && memcmp ( type,"MSK",3 ) == 0) {
            // On lit la ligne d'un masque
            is_mask = true;

            if (masks->size() == 0 || masks->back()) {
                // La première ligne ne peut être un masque et on ne peut pas avoir deux masques à la suite
                BOOST_LOG_TRIVIAL(error) <<  "A MSK line have to follow an IMG line" ;
                BOOST_LOG_TRIVIAL(error) <<  "\t line : " << line ;   
                return false;             
            }
        }
        else {
            BOOST_LOG_TRIVIAL(error) <<  "We have to read 8 values for IMG or 2 for MSK" ;
            BOOST_LOG_TRIVIAL(error) <<  "\t line : " << line ;
            return false;
        }

        char* path = (char*) malloc(IMAGE_MAX_FILENAME_LENGTH);
        memset ( path, 0, IMAGE_MAX_FILENAME_LENGTH );
        strcpy ( path,tmpPath );

        // On ajoute tout ça dans les vecteurs
        masks->push_back(is_mask);
        paths->push_back(path);
        bboxes->push_back(bb);
        resxs->push_back(resx);
        resys->push_back(resy);

    }

    if (file.eof()) {
        BOOST_LOG_TRIVIAL(debug) << "Fin du fichier de configuration atteinte";
        file.close();
        return true;
    } else {
        BOOST_LOG_TRIVIAL(error) << "Failure reading the configuration file " << configuration_path;
        file.close();
        return false;
    }
}


/**
 * \~french
 * \brief Charge les images en entrée et en sortie depuis le fichier de configuration
 * \details On va récupérer toutes les informations de toutes les images et masques présents dans le fichier de configuration et créer les objets FileImage correspondant. Toutes les images ici manipulées sont de vraies images (physiques) dans ce sens où elles sont des fichiers soit lus, soit qui seront écrits.
 *
 * Le chemin vers le fichier de configuration est stocké dans la variables globale configuration_path et images_root va être concaténer au chemin vers les fichiers de sortie.
 * \param[out] output_image image résultante de l'outil
 * \param[out] output_mask masque résultat de l'outil, si demandé
 * \param[out] input_images ensemble des images en entrée
 * \return code de retour, 0 si réussi, -1 sinon
 */
int load_images ( FileImage** output_image, FileImage** output_mask, std::vector<FileImage*>* input_images ) {
    
    std::vector<bool> masks;
    std::vector<char*> paths;
    std::vector<BoundingBox<double> > bboxes;
    std::vector<double> resxs;
    std::vector<double> resys;

    if (! load_configuration(&masks, &paths, &bboxes, &resxs, &resys) ) {
        BOOST_LOG_TRIVIAL(error) <<  "Cannot load configuration file " << configuration_path ;
        return -1;
    }

    // On doit avoir au moins deux lignes, trois si on a un masque de sortie
    if (masks.size() < 2 || (masks.size() == 2 && masks.back()) ) {
        BOOST_LOG_TRIVIAL(error) <<  "We have no input images in configuration file " << configuration_path ;
        return -1;
    }

    // On va charger les images en entrée en premier pour avoir certaines informations
    int first_input = 1;
    if (masks.at(1)) {
        // La deuxième ligne est le masque de sortie
        first_input = 2;
    }
    /****************** LES ENTRÉES : CRÉATION ******************/

    int input_count = 0;

    for ( int i = 0; i < paths.size(); i++ ) {
        BOOST_LOG_TRIVIAL(debug) << "paths[" << i << "] = " << paths.at(i);
    }

    for ( int i = first_input; i < masks.size(); i++ ) {
        BOOST_LOG_TRIVIAL(debug) << "image " << paths.at(i);

        input_count++;
        BOOST_LOG_TRIVIAL(debug) << "Input " << input_count;

        if ( resxs.at(i) == 0. || resys.at(i) == 0.) {
            BOOST_LOG_TRIVIAL(error) <<  "Source image " << input_count << " is not valid (resolutions)" ;
            return -1;
        }

        FileImage* input_image = FileImage::create_to_read ( paths.at(i), bboxes.at(i), resxs.at(i), resys.at(i) );
        if ( input_image == NULL ) {
            BOOST_LOG_TRIVIAL(error) <<  "Impossible de creer une image a partir de " << paths.at(i) ;
            return -1;
        }

        delete paths.at(i);

        if ( i+1 < masks.size() && masks.at(i+1) ) {
            
            FileImage* input_mask = FileImage::create_to_read ( paths.at(i+1), bboxes.at(i), resxs.at(i), resys.at(i) );
            if ( input_mask == NULL ) {
                BOOST_LOG_TRIVIAL(error) <<  "Impossible de creer un masque a partir de " << paths.at(i) ;
                return -1;
            }

            if ( ! input_image->set_mask ( input_mask ) ) {
                BOOST_LOG_TRIVIAL(error) <<  "Cannot add mask to the input FileImage" ;
                return -1;
            }
            i++;
            delete paths.at(i);
        }

        input_images->push_back ( input_image );

        /* On vérifie que le format des canaux est le même pour toutes les images en entrée :
         *     - sample_format
         *     - samplesperpixel
         */

        if (! output_format_provided && input_count == 1) {
            /* On n'a pas précisé de format en sortie, on va donc utiliser celui des entrées
             * On veut donc avoir le même format pour toutes les entrées
             * On lit la première image en entrée, qui sert de référence
             * L'image en sortie sera à ce format
             */
            samplesperpixel = input_image->get_channels();
            sample_format = input_image->get_sample_format();
        } else if (! output_format_provided) {
            // On doit avoir le même format pour tout le monde
            if (samplesperpixel != input_image->get_channels()) {
                BOOST_LOG_TRIVIAL(error) << "We don't provided output format, so all inputs have to own the same" ;
                BOOST_LOG_TRIVIAL(error) << "The first image and the " << input_count << " one don't have the same number of samples per pixel" ;
                BOOST_LOG_TRIVIAL(error) << samplesperpixel << " != " << input_image->get_channels() ;
                return -1;
            }
            if (sample_format != input_image->get_sample_format()) {
                BOOST_LOG_TRIVIAL(error) << "We don't provided output format, so all inputs have to own the same" ;
                BOOST_LOG_TRIVIAL(error) << "The first image and the " << input_count << " one don't have the same sample format" ;
                BOOST_LOG_TRIVIAL(error) << sample_format << " != " << input_image->get_sample_format() ;
                return -1;
            }
        }
    }

    if ( input_images->size() == 0 ) {
        BOOST_LOG_TRIVIAL(error) <<  "Erreur lecture du fichier de parametres '" << configuration_path << "' : pas de données en entrée." ;
        return -1;
    } else {
        BOOST_LOG_TRIVIAL(debug) <<  input_count << " image(s) en entrée" ;
    }

    /********************** LA SORTIE : CRÉATION *************************/

    if (samplesperpixel == 1) {
        photometric = Photometric::GRAY;
    } else if (samplesperpixel == 2) {
        photometric = Photometric::GRAY;
    } else {
        photometric = Photometric::RGB;
    }

    // Arrondi a la valeur entiere la plus proche
    int width = lround ( ( bboxes.at(0).xmax - bboxes.at(0).xmin ) / ( resxs.at(0) ) );
    int height = lround ( ( bboxes.at(0).ymax - bboxes.at(0).ymin ) / ( resys.at(0) ) );

    *output_image = FileImage::create_to_write (
        paths.at(0), bboxes.at(0), resxs.at(0), resys.at(0), width, height,
        samplesperpixel, sample_format, photometric, compression
    );

    if ( *output_image == NULL ) {
        BOOST_LOG_TRIVIAL(error) <<  "Impossible de creer l'image " << paths.at(0) ;
        return -1;
    }

    delete paths.at(0);

    if ( first_input == 2 ) {

        *output_mask = FileImage::create_to_write (
            paths.at(1), bboxes.at(0), resxs.at(0), resys.at(0), width, height,
            1, SampleFormat::UINT8, Photometric::MASK, Compression::DEFLATE
        );

        if ( *output_mask == NULL ) {
            BOOST_LOG_TRIVIAL(error) <<  "Impossible de creer le masque " << paths.at(1) ;
            return -1;
        }

        delete paths.at(1);
    }

    if (debug_logger) ( *output_image )->print();

    return 0;
}

int add_converters(std::vector<FileImage*> input_images) {
    if (! output_format_provided) {
        // On n'a pas précisé de format en sortie, donc toutes les images doivent avoir le même
        // Et la sortie a aussi ce format, donc pas besoin de convertisseur

        return 0;
    }

    for ( std::vector<FileImage*>::iterator input_images_iterator = input_images.begin(); input_images_iterator < input_images.end(); input_images_iterator++ ) {

        if ( ! ( *input_images_iterator )->add_converter ( sample_format, samplesperpixel ) ) {
            BOOST_LOG_TRIVIAL(error) << "Cannot add converter for an input image";
            ( *input_images_iterator )->print();
            return -1;
        }

        if (debug_logger) ( *input_images_iterator )->print();
    }

    return 0;

}

/**
 * \~french
 * \brief Trie les images sources
 * \details La première image en entrée peut être une image de fond : elle est alors compatible avec l'image en sortie
 * Les images suivantes sont les images à "décimer", dont on ne veut garder qu'un pixel sur N. Elles sont compatibles entre elles mais pas
 * avec l'image en sortie (et donc pas avec l'image de fond).
 * 
 * On doit donc avoir à la fin soit un paquet, soit deux avec une seule image dans le premier (le fond)
 *
 * \param[in] input_images images en entrée
 * \param[out] sorted_input_images images en entrée, triées en paquets compatibles
 * \return code de retour, 0 si réussi, -1 sinon
 */
int sort_images ( std::vector<FileImage*> input_images, std::vector<std::vector<Image*> >* sorted_input_images ) {
    std::vector<Image*> current_pack;
    std::vector<FileImage*>::iterator current_input_images_iterator = input_images.begin();

    /* we create consistent images' vectors (X/Y resolution and X/Y phases)
     * Masks are moved in parallel with images
     */
    for ( std::vector<FileImage*>::iterator input_images_iterator = input_images.begin(); input_images_iterator < input_images.end()-1; input_images_iterator++ ) {

        if ( ! ( *input_images_iterator )->compatible ( * ( input_images_iterator+1 ) ) ) {
            // two following images are not compatible, we split images' vector
            current_pack.assign ( current_input_images_iterator,input_images_iterator+1 );
            current_input_images_iterator = input_images_iterator+1;
            sorted_input_images->push_back ( current_pack );
        }
    }

    // we don't forget to store last images in sorted_input_images
    // images
    current_pack.assign ( current_input_images_iterator,input_images.end() );
    sorted_input_images->push_back ( current_pack );
    
    if (! (sorted_input_images->size() == 1 || sorted_input_images->size() == 2) ) {
        BOOST_LOG_TRIVIAL(error) << "Input images have to constitute 1 or 2 (the background) consistent images' pack";
        return -1;
    }
    
    if (sorted_input_images->size() == 2) {
        if (sorted_input_images->at(0).size() != 1) {
            BOOST_LOG_TRIVIAL(error) << "If a background image is present, no another consistent image with it (one image pack)";
            return -1;
        }
    }

    return 0;
}


/**
 * \~french \brief Traite chaque paquet d'images en entrée
 * \~english \brief Treat each input images pack
 * \~french \details 
 *
 * \param[in] output_image image de sortie
 * \param[in] sorted_input_images paquets d'images en entrée
 * \param[out] merged_image paquet d'images superposable avec l'image de sortie
 * \param[in] nodata valeur de non-donnée
 * \return 0 en cas de succès, -1 en cas d'erreur
 */
int merge_images ( FileImage* output_image, // Sortie
                     std::vector<std::vector<Image*> >& sorted_input_images, // Entrée
                     ExtendedCompoundImage** merged_image, // Résultat du merge
                     int* nodata ) {

    std::vector<Image*> stackable_images;
    
    // ************* Le fond (éventuel)
    if ( sorted_input_images.size() == 2) {
        BOOST_LOG_TRIVIAL(debug) << "We have a background";
        if ( ! sorted_input_images.at(0).at(0)->compatible ( output_image ) ) {
            BOOST_LOG_TRIVIAL(error) <<  "Background image have to be consistent with the output image" ;
            sorted_input_images.at(0).at(0)->print();
            BOOST_LOG_TRIVIAL(error) << "not consistent with";
            output_image->print();
            return -1;
        }
        stackable_images.push_back ( sorted_input_images.at(0).at(0) );
    }
    
    // ************* Les images à décimer

    // L'image
    ExtendedCompoundImage* image_to_decimate = ExtendedCompoundImage::create ( sorted_input_images.at(sorted_input_images.size() - 1), nodata, 0 );
    if ( image_to_decimate == NULL ) {
        BOOST_LOG_TRIVIAL(error) <<  "Impossible d'assembler les images en entrée" ;
        return -1;
    }
    
    DecimatedImage* decimated_image = DecimatedImage::create(image_to_decimate, output_image->get_bbox(), output_image->get_resx(), output_image->get_resy(), nodata);
    if ( decimated_image == NULL ) {
        BOOST_LOG_TRIVIAL(error) <<  "Impossible de créer la DecimatedImage (image)" ;
        return -1;
    }

    // Le masque
    ExtendedCompoundMask* make_to_decimate = new ExtendedCompoundMask ( image_to_decimate );
    if ( ! image_to_decimate->set_mask ( make_to_decimate ) ) {
        BOOST_LOG_TRIVIAL(error) <<  "Cannot add mask to the compound image " ;
        return -1;
    }
    
    int nodata_mask[1] = {0};
    DecimatedImage* decimated_mask = DecimatedImage::create(make_to_decimate, output_image->get_bbox(), output_image->get_resx(), output_image->get_resy(), nodata_mask);
    if ( decimated_mask == NULL ) {
        BOOST_LOG_TRIVIAL(error) <<  "Impossible de créer la DecimatedImage (mask)" ;
        return -1;
    }
    
    if ( ! decimated_image->set_mask ( decimated_mask ) ) {
        BOOST_LOG_TRIVIAL(error) <<  "Cannot add mask to the DecimatedImage" ;
        return -1;
    }
    
    if (debug_logger) image_to_decimate->print();
    if (debug_logger) decimated_image->print();
    
    stackable_images.push_back(decimated_image);

    // Assemblage des paquets et decoupage aux dimensions de l image de sortie
    *merged_image = ExtendedCompoundImage::create (
        output_image->get_width(), output_image->get_height(), output_image->get_channels(), output_image->get_bbox(),
        stackable_images, nodata, 0
    );

    if ( *merged_image == NULL ) {
        BOOST_LOG_TRIVIAL(error) <<  "Cannot create final compounded image." ;
        return -1;
    }

    // Masque
    ExtendedCompoundMask* merged_mask = new ExtendedCompoundMask ( *merged_image );

    if ( ! ( *merged_image )->set_mask ( merged_mask ) ) {
        BOOST_LOG_TRIVIAL(error) <<  "Cannot add mask to the main Extended Compound Image" ;
        return -1;
    }

    return 0;
}

/**
 ** \~french
 * \brief Fonction principale de l'outil mergeNtiff
 * \param[in] argc nombre de paramètres
 * \param[in] argv tableau des paramètres
 * \return code de retour, 0 si réussi, -1 sinon
 ** \~english
 * \brief Main function for tool mergeNtiff
 * \param[in] argc parameters number
 * \param[in] argv parameters array
 * \return 0 if success, -1 otherwise
 */
int main ( int argc, char **argv ) {

    FileImage* output_image ;
    FileImage* output_mask = NULL;
    std::vector<FileImage*> input_images;
    std::vector<std::vector<Image*> > sorted_input_images;
    ExtendedCompoundImage* merged_image;

    /* Initialisation des Loggers */
    boost::log::core::get()->set_filter( boost::log::trivial::severity >= boost::log::trivial::info );
    logging::add_common_attributes();
    boost::log::register_simple_formatter_factory< boost::log::trivial::severity_level, char >("Severity");
    logging::add_console_log (
        std::cout,
        keywords::format = "%Severity%\t%Message%"
    );

    // Lecture des parametres de la ligne de commande
    if ( parse_command_line ( argc, argv ) < 0 ) {
        error ( "Echec lecture ligne de commande",-1 );
    }

    // On sait maintenant si on doit activer le niveau de log DEBUG
    if (debug_logger) {
        boost::log::core::get()->set_filter( boost::log::trivial::severity >= boost::log::trivial::debug );
    }

    // On regarde si on a tout précisé en sortie, pour voir si des conversions sont possibles
    if (sample_format != SampleFormat::UNKNOWN && samplesperpixel != 0) {
      output_format_provided = true;
    }

    BOOST_LOG_TRIVIAL(debug) <<  "Load" ;
    // Chargement des images
    if ( load_images ( &output_image, &output_mask, &input_images ) < 0 ) {
        error ( "Echec chargement des images",-1 );
    }

    BOOST_LOG_TRIVIAL(debug) <<  "Add converters" ;
    // Ajout des modules de conversion aux images en entrée
    if ( add_converters ( input_images ) < 0 ) {
        error ( "Echec ajout des convertisseurs", -1 );
    }
    
    // Conversion string->int[] du paramètre nodata
    BOOST_LOG_TRIVIAL(debug) <<  "Nodata interpretation" ;
    int spp = input_images.at(0)->get_channels();
    int nodata[spp];

    char* charValue = strtok ( strnodata,"," );
    if ( charValue == NULL ) {
        error ( "Error with option -n : a value for nodata is missing",-1 );
    }
    nodata[0] = atoi ( charValue );
    for ( int i = 1; i < spp; i++ ) {
        charValue = strtok ( NULL, "," );
        if ( charValue == NULL ) {
            error ( "Error with option -n : a value for nodata is missing",-1 );
        }
        nodata[i] = atoi ( charValue );
    }

    BOOST_LOG_TRIVIAL(debug) <<  "Sort" ;
    // Tri des images
    if ( sort_images ( input_images, &sorted_input_images ) < 0 ) {
        error ( "Echec tri des images",-1 );
    }

    BOOST_LOG_TRIVIAL(debug) <<  "Merge" ;
    // Fusion des paquets d images
    if ( merge_images ( output_image, sorted_input_images, &merged_image, nodata ) < 0 ) {
        error ( "Echec fusion des paquets d images",-1 );
    }

    BOOST_LOG_TRIVIAL(debug) <<  "Save image" ;
    // Enregistrement de l'image fusionnée
    if ( output_image->write_image ( merged_image ) < 0 ) {
        error ( "Echec enregistrement de l image finale",-1 );
    }

    if ( output_mask != NULL ) {
        BOOST_LOG_TRIVIAL(debug) <<  "Save mask" ;
        // Enregistrement du masque fusionné, si demandé
        if ( output_mask->write_image ( merged_image->Image::get_mask() ) < 0 ) {
            error ( "Echec enregistrement du masque final",-1 );
        }
    }

    BOOST_LOG_TRIVIAL(debug) <<  "Clean" ;

    CrsBook::clean_crss();
    ProjPool::clean_projs();
    proj_cleanup();
    
    delete merged_image;
    delete output_image;
    delete output_mask;

    return 0;
}

