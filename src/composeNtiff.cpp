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
 * \file composeNtiff.cpp
 * \author Institut national de l'information géographique et forestière
 * \~french \brief Montage de N images TIFF aux mêmes dimensions et caractéristiques
 * \~english \brief Monte N TIFF images with same dimensions and attributes
 */

#include <iostream>
#include <cstdlib>
#include <stdio.h>
#include <dirent.h>
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
#include <rok4/image/CompoundImage.h>
#include <rok4/enums/Format.h>
#include <rok4/utils/Cache.h>
#include "config.h"

/** \~french Nombre d'images dans le sens de la largeur */
int images_widthwise = 0;
/** \~french Nombre d'images dans le sens de la hauteur */
int images_heightwise = 0;

/** \~french Compression de l'image de sortie */
Compression::eCompression compression = Compression::NONE;

/** \~french Dossier des images sources */
char* input_directory_path = 0;

/** \~french Chemin de l'image en sortie */
char* output_image_path = 0;

/** \~french Activation du niveau de log debug. Faux par défaut */
bool debug_logger=false;

/** \~french Message d'usage de la commande pbf2cache */
std::string help = std::string("\ncomposeNtiff version ") + std::string(VERSION) + "\n\n"
    "Monte N TIFF image, forming a regular grid\n\n"

    "Usage: composeNtiff -s <DIRECTORY> -g <VAL> <VAL> -c <VAL> <OUTPUT FILE>\n\n"

    "Parameters:\n"
    "     -s source directory. All file into have to be images. If too much images are present, first are used.\n"
    "     -c output compression : default value : none\n"
    "             raw     no compression\n"
    "             none    no compression\n"
    "             jpg     Jpeg encoding (quality 75)\n"
    "             jpg90   Jpeg encoding (quality 90)\n"
    "             lzw     Lempel-Ziv & Welch encoding\n"
    "             pkb     PackBits encoding\n"
    "             zip     Deflate encoding\n"
    "     -g number of images, widthwise and heightwise, to compose the final image\n"
    "     -d debug logger activation\n\n"

    "Example\n"
    "     composeNtiff -s /home/ign/sources -g 10 10 -c zip output.tif\n\n";
    

/**
 * \~french
 * \brief Affiche l'utilisation et les différentes options de la commande composeNtiff #help
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
            case 's': // Input directory
                if ( i++ >= argc ) {
                    BOOST_LOG_TRIVIAL(error) <<  "Error id -s option" ;
                    return -1;
                }
                input_directory_path = argv[i];
                break;
            case 'c': // compression
                if ( ++i == argc ) {
                    BOOST_LOG_TRIVIAL(error) << "Error in -c option" ;
                    return -1;
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
                    BOOST_LOG_TRIVIAL(error) <<  "Unknown compression : " + argv[i][1] ;
                    return -1;
                }
                break;
            case 'g':
                if ( i+2 >= argc ) {
                    BOOST_LOG_TRIVIAL(error) <<  "Error in -g option" ;
                    return -1;
                }
                images_widthwise = atoi ( argv[++i] );
                images_heightwise = atoi ( argv[++i] );
                break;
            default:
                BOOST_LOG_TRIVIAL(error) <<  "Unknown option : " << argv[i] ;
                return -1;
            }
        } else {
            if ( output_image_path == 0 ) output_image_path = argv[i];
            else {
                BOOST_LOG_TRIVIAL(error) <<  "Argument must specify just ONE output file" ;
                return -1;
            }
        }
    }

    // Input directory control
    if ( input_directory_path == 0 ) {
        BOOST_LOG_TRIVIAL(error) <<  "We need to have a source images' directory (option -s)" ;
        return -1;
    }

    // Output file control
    if ( output_image_path == 0 ) {
        BOOST_LOG_TRIVIAL(error) <<  "We need to have an output file" ;
        return -1;
    }

    // Geometry control
    if ( images_widthwise == 0 || images_heightwise == 0) {
        BOOST_LOG_TRIVIAL(error) <<  "We need to know composition geometry (option -g)" ;
        return -1;
    }

    return 0;
}


/**
 * \~french
 * \brief Charge les images contenues dans le dossier en entrée et l'image de sortie
 * \details Toutes les images doivent avoir les mêmes caractéristiques, dimensions et type des canaux. Les images en entrée seront gérée par un objet de la classe #CompoundImage, et l'image en sortie sera une image TIFF.
 *
 * \param[out] output_image image résultante de l'outil
 * \param[out] compound_image ensemble des images en entrée
 * \return code de retour, 0 si réussi, -1 sinon
 */
int load_images ( FileImage** output_image, CompoundImage** compound_image ) {

    std::vector< std::string > images_filenames;
    
    std::vector< std::vector<Image*> > input_images;

    // Dimensionnement de input_images
    input_images.resize(images_heightwise);
    for (int row = 0; row < images_heightwise; row++)
        input_images.at(row).resize(images_widthwise);
    for ( int i = 0; i < images_heightwise; i++ ) for ( int j = 0; j < images_widthwise; j++ ) input_images[i][j] = NULL;

    int width, height;
    int samplesperpixel;
    SampleFormat::eSampleFormat sample_format;
    Photometric::ePhotometric photometric;

    /********* Parcours du dossier ************/

    // Ouverture et test du répertoire source
    DIR * input_directory = opendir(input_directory_path);

    if (input_directory == NULL) {
        BOOST_LOG_TRIVIAL(error) << "Cannot open input directory : " << input_directory_path;
        return -1;
    }
    
    struct dirent * ent;

     // On récupère tous les fichiers, puis on les trie
    while ((ent = readdir(input_directory)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        images_filenames.push_back(std::string(ent->d_name));
    }

    closedir(input_directory);
    
    BOOST_LOG_TRIVIAL(debug) << images_filenames.size() << " files in the provided directory";
    if (images_filenames.size() > images_widthwise*images_heightwise) {
        BOOST_LOG_TRIVIAL(warning) << "We have too much images in the input directory (regarding to the provided geometry).";
        BOOST_LOG_TRIVIAL(warning) << "Only " << images_widthwise*images_heightwise << " first images will be used";
    }

    if (images_filenames.size() < images_widthwise*images_heightwise) {
        BOOST_LOG_TRIVIAL(error) << "Not enough images, we need " << images_widthwise*images_heightwise << ", and we find " << images_filenames.size();
        return -1;
    }

    std::sort(images_filenames.begin(), images_filenames.end());

    /********* Chargement des images ************/
    
    /* On doit connaître les dimensions des images en entrée pour pouvoir créer les images de sortie
     * Lecture et création des images sources */
    for (int k = 0; k < images_widthwise*images_heightwise; k++) {

        int i = k/images_widthwise;
        int j = k%images_widthwise;

        std::string str = input_directory_path + images_filenames.at(k);
        char filename[256];
        memset(filename, 0, 256);
        memcpy(filename, str.c_str(), str.length());

        FileImage* pImage = FileImage::create_to_read (filename, BoundingBox<double>(0,0,0,0), -1., -1. );
        if ( pImage == NULL ) {
            BOOST_LOG_TRIVIAL(error) <<  "Cannot create a FileImage from the file " << filename ;
            return -1;
        }

        if ( i == 0 && j == 0 ) {
            // C'est notre première image en entrée, on mémorise les caractéristiques
            sample_format = pImage->get_sample_format();
            photometric = pImage->get_photometric();
            samplesperpixel = pImage->get_channels();
            width = pImage->get_width();
            height = pImage->get_height();
        } else {
            // Toutes les images en entrée doivent avoir certaines caractéristiques en commun
            if ( sample_format != pImage->get_sample_format() ||
                 photometric != pImage->get_photometric() ||
                 samplesperpixel != pImage->get_channels() ||
                 width != pImage->get_width() ||
                 height != pImage->get_height() )
            {
                delete pImage;
                for ( int ii = 0; ii < images_heightwise; ii++ ) for ( int jj = 0; jj < images_widthwise; jj++ ) delete input_images[ii][jj];
                BOOST_LOG_TRIVIAL(error) <<  "All input images must have same dimensions and sample type : error for image " << filename ;
                return -1;
            }
        }

        pImage->set_bbox(BoundingBox<double>(j * width, (images_heightwise - i - 1) * height, (j+1) * width, (images_heightwise - i) * height));
        
        input_images[i][j] = pImage;
        
    }

    *compound_image = new CompoundImage(input_images);

    // Création de l'image de sortie
    *output_image = FileImage::create_to_write (
        output_image_path, BoundingBox<double>(0., 0., 0., 0.), -1., -1., width*images_widthwise, height*images_heightwise, samplesperpixel,
        sample_format, photometric,compression
    );

    if ( *output_image == NULL ) {
        BOOST_LOG_TRIVIAL(error) <<  "Impossible de creer l'image de sortie " << output_image_path ;
        return -1;
    }

    return 0;
}

/**
 ** \~french
 * \brief Fonction principale de l'outil composeNtiff
 * \param[in] argc nombre de paramètres
 * \param[in] argv tableau des paramètres
 * \return code de retour, 0 si réussi, -1 sinon
 ** \~english
 * \brief Main function for tool composeNtiff
 * \param[in] argc parameters number
 * \param[in] argv parameters array
 * \return 0 if success, -1 otherwise
 */
int main ( int argc, char **argv ) {

    FileImage* output_image = NULL;
    CompoundImage* compound_image = NULL;

    /* Initialisation des Loggers */
    boost::log::core::get()->set_filter( boost::log::trivial::severity >= boost::log::trivial::info );
    logging::add_common_attributes();
    boost::log::register_simple_formatter_factory< boost::log::trivial::severity_level, char >("Severity");
    logging::add_console_log (
        std::cout,
        keywords::format = "%Severity%\t%Message%"
    );

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
    if ( load_images ( &output_image, &compound_image ) < 0 ) {
        if ( compound_image ) {
            delete compound_image;
        }
        if ( output_image ) {
            delete output_image; 
        }
        error ( "Cannot load images from the input directory",-1 );
    }

    BOOST_LOG_TRIVIAL(debug) <<  "Save image" ;
    // Enregistrement de l'image fusionnée
    if ( output_image->write_image ( compound_image ) < 0 ) {
        error ( "Cannot write the compound image",-1 );
    }

    CrsBook::clean_crss();
    ProjPool::clean_projs();
    proj_cleanup();
    delete compound_image;
    delete output_image;

    return 0;
}
