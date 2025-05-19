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
 * \file mergeNtiff.cpp
 * \author Institut national de l'information géographique et forestière
 * \~french \brief Création d'une image TIFF géoréférencée à partir de n images sources géoréférencées
 * \~english \brief Create one georeferenced TIFF image from several georeferenced images
 * \~ \image html mergeNtiff.png \~french
 *
 * La légende utilisée dans tous les schémas de la documentation de ce fichier sera la suivante
 * \~ \image html mergeNtiff_legende.png \~french
 *
 * Pour réaliser la fusion des images en entrée, on traite différemment :
 * \li les images qui sont superposables à l'image de sortie (même SRS, mêmes résolutions, mêmes phases) : on parle alors d'images compatibles, pas de réechantillonnage nécessaire.
 * \li les images non compatibles mais de même SRS : un passage par le réechantillonnage (plus lourd en calcul) est indispensable.
 * \li les images non compatibles et de SRS différents : un passage par la reprojection (encore plus lourd en calcul) est indispensable.
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <tiffio.h>
namespace logging = boost::log;
namespace keywords = boost::log::keywords;

#include <rok4/utils/CRS.h>
#include <rok4/image/ExtendedCompoundImage.h>
#include <rok4/image/file/FileImage.h>
#include <rok4/enums/Format.h>
#include <rok4/utils/Cache.h>
#include <rok4/enums/Interpolation.h>
#include <rok4/processors/PixelConverter.h>
#include <rok4/image/ReprojectedImage.h>
#include <rok4/image/ResampledImage.h>
#include <math.h>
#include "config.h"

#include <rok4/style/Style.h>
#include <rok4/image/PaletteImage.h>
#include <rok4/image/EstompageImage.h>
#include <rok4/image/PenteImage.h>
#include <rok4/image/AspectImage.h>

// Paramètres de la ligne de commande déclarés en global
/** \~french Chemin du fichier de configuration des images */
char configuration_path[256];
/** \~french Racine pour les images de sortie */
char images_root[256];

/** \~french Valeur de nodata sous forme de chaîne de caractère (passée en paramètre de la commande) */
char strnodata[256];
int* nodata;
/** \~french A-t-on précisé une valeur de nodata */
bool nodata_provided = false;

/** \~french A-t-on précisé le format en sortie, c'est à dire les 3 informations samplesperpixel et sampleformat */
bool output_format_provided = false;
/** \~french Nombre de canaux par pixel, pour l'image en sortie */
uint16_t samplesperpixel = 0;
/** \~french Format du canal (entier, flottant, signé ou non...), pour l'image en sortie */
SampleFormat::eSampleFormat sampleformat = SampleFormat::UNKNOWN;

/** \~french Photométrie (rgb, gray), déduit du nombre de canaux */
Photometric::ePhotometric photometric;

/** \~french Compression de l'image de sortie */
Compression::eCompression compression = Compression::NONE;
/** \~french Interpolation utilisée pour le réechantillonnage ou la reprojection */
Interpolation::KernelType interpolation = Interpolation::CUBIC;

/** \~french A-t-on précisé une image de fond */
bool background_provided = false;
/** \~french A-t-on précisé un style */
bool style_provided = false;
/** \~french Chemin du fichier de style à appliquer */
char style_file[256];
/** \~french Objet style à appliquer */
Style* style;

/** \~french Activation du niveau de log debug. Faux par défaut */
bool debug_logger = false;

/** \~french Message d'usage de la commande mergeNtiff */
std::string help = std::string("\nmergeNtiff version ") + std::string(VERSION) +
                   "\n\n"

                   "Create one georeferenced TIFF image from several georeferenced TIFF images.\n\n"

                   "Usage: mergeNtiff -f <FILE> [-r <DIR>] -c <VAL> -i <VAL> -n <VAL> [-a <VAL> -s <VAL> -b <VAL>]\n"

                   "Parameters:\n"
                   "    -f configuration file : list of output and source images and masks\n"
                   "    -g : first input is a background image\n"
                   "    -r output root : root directory for output files, have to end with a '/'\n"
                   "    -c output compression :\n"
                   "            raw     no compression\n"
                   "            none    no compression\n"
                   "            jpg     Jpeg encoding\n"
                   "            lzw     Lempel-Ziv & Welch encoding\n"
                   "            pkb     PackBits encoding\n"
                   "            zip     Deflate encoding\n"
                   "    -i interpolation : used for resampling :\n"
                   "            nn nearest neighbor\n"
                   "            linear\n"
                   "            bicubic\n"
                   "            lanczos lanczos 3\n"
                   "    -n nodata value, one interger per sample, seperated with comma. If a style is provided, nodata values will be read from style. Examples\n"
                   "            -99999 for DTM\n"
                   "            255,255,255 for orthophotography\n"
                   "    -p style file\n"
                   "    -a sample format : (float32 or uint8)\n"
                   "    -s samples per pixel : (1, 2, 3 or 4)\n"
                   "    -d debug logger activation\n\n"

                   "If sampleformat or samplesperpixel are not provided, those informations are read from the image sources (all have to own the same). If all are provided, conversion may be done.\n\n"

                   "Examples\n"
                   "    - for orthophotography\n"
                   "    mergeNtiff -f conf.txt -c zip -i bicubic -n 255,255,255\n"
                   "    - for DTM\n"
                   "    mergeNtiff -f conf.txt -c zip -i nn -s 1 -p gray -a float32 -n -99999\n\n";

/**
 * \~french
 * \brief Affiche l'utilisation et les différentes options de la commande mergeNtiff #help
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
void error(std::string message, int error_code) {
    BOOST_LOG_TRIVIAL(error) << message;
    BOOST_LOG_TRIVIAL(error) << "Configuration file : " << configuration_path;
    usage();
    exit(error_code);
}

/**
 * \~french
 * \brief Récupère les valeurs passées en paramètres de la commande, et les stocke dans les variables globales
 * \param[in] argc nombre de paramètres
 * \param[in] argv tableau des paramètres
 * \return code de retour, 0 si réussi, -1 sinon
 */
int parse_command_line(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
                case 'h':  // help
                    usage();
                    exit(0);
                case 'd':  // debug logs
                    debug_logger = true;
                    break;
                case 'g':  // background
                    background_provided = true;
                    break;
                case 'f':  // fichier de liste des images source
                    if (i++ >= argc) {
                        BOOST_LOG_TRIVIAL(error) << "Error in option -f";
                        return -1;
                    }
                    strcpy(configuration_path, argv[i]);
                    break;
                case 'r':  // racine pour le fichier de configuration
                    if (i++ >= argc) {
                        BOOST_LOG_TRIVIAL(error) << "Error in option -r";
                        return -1;
                    }
                    strcpy(images_root, argv[i]);
                    break;
                case 'i':  // interpolation
                    if (i++ >= argc) {
                        BOOST_LOG_TRIVIAL(error) << "Error in option -i";
                        return -1;
                    }
                    if (strncmp(argv[i], "lanczos", 7) == 0)
                        interpolation = Interpolation::LANCZOS_3;
                    else if (strncmp(argv[i], "nn", 2) == 0)
                        interpolation = Interpolation::NEAREST_NEIGHBOUR;
                    else if (strncmp(argv[i], "bicubic", 7) == 0)
                        interpolation = Interpolation::CUBIC;
                    else if (strncmp(argv[i], "linear", 6) == 0)
                        interpolation = Interpolation::LINEAR;
                    else {
                        BOOST_LOG_TRIVIAL(error) << "Unknown value for option -i : " << argv[i];
                        return -1;
                    }
                    break;
                case 'n':  // nodata
                    if (i++ >= argc) {
                        BOOST_LOG_TRIVIAL(error) << "Error in option -n";
                        return -1;
                    }
                    strcpy(strnodata, argv[i]);
                    nodata_provided = true;
                    break;
                case 'c':  // compression
                    if (i++ >= argc) {
                        BOOST_LOG_TRIVIAL(error) << "Error in option -c";
                        return -1;
                    }
                    if (strncmp(argv[i], "raw", 3) == 0)
                        compression = Compression::NONE;
                    else if (strncmp(argv[i], "none", 4) == 0)
                        compression = Compression::NONE;
                    else if (strncmp(argv[i], "zip", 3) == 0)
                        compression = Compression::DEFLATE;
                    else if (strncmp(argv[i], "pkb", 3) == 0)
                        compression = Compression::PACKBITS;
                    else if (strncmp(argv[i], "jpg", 3) == 0)
                        compression = Compression::JPEG;
                    else if (strncmp(argv[i], "lzw", 3) == 0)
                        compression = Compression::LZW;
                    else {
                        BOOST_LOG_TRIVIAL(error) << "Unknown value for option -c : " << argv[i];
                        return -1;
                    }
                    break;

                case 'p':  // style
                    if (i++ >= argc) {
                        BOOST_LOG_TRIVIAL(error) << "Error in option -p";
                        return -1;
                    }
                    strcpy(style_file, argv[i]);
                    style_provided = true;
                    break;

                /****************** OPTIONNEL, POUR FORCER DES CONVERSIONS **********************/
                case 's':  // samplesperpixel
                    if (i++ >= argc) {
                        BOOST_LOG_TRIVIAL(error) << "Error in option -s";
                        return -1;
                    }
                    if (strncmp(argv[i], "1", 1) == 0)
                        samplesperpixel = 1;
                    else if (strncmp(argv[i], "2", 1) == 0)
                        samplesperpixel = 2;
                    else if (strncmp(argv[i], "3", 1) == 0)
                        samplesperpixel = 3;
                    else if (strncmp(argv[i], "4", 1) == 0)
                        samplesperpixel = 4;
                    else {
                        BOOST_LOG_TRIVIAL(error) << "Unknown value for option -s : " << argv[i];
                        return -1;
                    }
                    break;
                case 'a':  // sampleformat
                    if (i++ >= argc) {
                        BOOST_LOG_TRIVIAL(error) << "Error in option -a";
                        return -1;
                    }
                    if (strncmp(argv[i], "uint8", 5) == 0)
                        sampleformat = SampleFormat::UINT8;
                    else if (strncmp(argv[i], "float32", 7) == 0)
                        sampleformat = SampleFormat::FLOAT32;
                    else {
                        BOOST_LOG_TRIVIAL(error) << "Unknown value for option -a : " << argv[i];
                        return -1;
                    }
                    break;
                    /*******************************************************************************/

                default:
                    BOOST_LOG_TRIVIAL(error) << "Unknown option : -" << argv[i][1];
                    return -1;
            }
        }
    }

    BOOST_LOG_TRIVIAL(debug) << "mergeNtiff -f " << configuration_path;

    return 0;
}

/**
 * \~french
 * \brief Lit l'ensemble de la configuration
 * \details On parse la ligne courante du fichier de configuration, en stockant les valeurs dans les variables fournies. On saute les lignes vides. On lit ensuite la ligne suivante :
 * \li si elle correspond à un masque, on complète les informations
 * \li si elle ne correspond pas à un masque, on recule le pointeur
 *
 * \param[in,out] masks Indicateurs de présence d'un masque
 * \param[in,out] paths Chemins des images
 * \param[in,out] srss Systèmes de coordonnées des images
 * \param[in,out] bboxes Rectangles englobant des images
 * \param[in,out] resxs Résolution en x des images
 * \param[in,out] resys Résolution en y des images
 * \return true en cas de succès, false si échec
 */
bool load_configuration(
    std::vector<bool>* masks,
    std::vector<char*>* paths,
    std::vector<std::string>* srss,
    std::vector<BoundingBox<double> >* bboxes,
    std::vector<double>* resxs,
    std::vector<double>* resys) {
    std::ifstream file;
    int rootLength = strlen(images_root);

    file.open(configuration_path);
    if (!file.is_open()) {
        BOOST_LOG_TRIVIAL(error) << "Impossible d'ouvrir le fichier " << configuration_path;
        return false;
    }

    while (file.good()) {
        char tmpPath[IMAGE_MAX_FILENAME_LENGTH];
        char tmpCRS[20];
        char line[2 * IMAGE_MAX_FILENAME_LENGTH];
        memset(line, 0, 2 * IMAGE_MAX_FILENAME_LENGTH);
        memset(tmpPath, 0, IMAGE_MAX_FILENAME_LENGTH);
        memset(tmpCRS, 0, 20);

        char type[3];
        std::string crs;
        BoundingBox<double> bb(0., 0., 0., 0.);
        double resx, resy;
        bool is_mask;

        file.getline(line, 2 * IMAGE_MAX_FILENAME_LENGTH);
        if (strlen(line) == 0) {
            continue;
        }
        int nb = std::sscanf(line, "%s %s %s %lf %lf %lf %lf %lf %lf", type, tmpPath, tmpCRS, &bb.xmin, &bb.ymax, &bb.xmax, &bb.ymin, &resx, &resy);
        if (nb == 9 && memcmp(type, "IMG", 3) == 0) {
            // On lit la ligne d'une image
            crs.assign(tmpCRS);
            is_mask = false;
        } else if (nb == 2 && memcmp(type, "MSK", 3) == 0) {
            // On lit la ligne d'un masque
            is_mask = true;

            if (masks->size() == 0 || masks->back()) {
                // La première ligne ne peut être un masque et on ne peut pas avoir deux masques à la suite
                BOOST_LOG_TRIVIAL(error) << "A MSK line have to follow an IMG line";
                BOOST_LOG_TRIVIAL(error) << "\t line : " << line;
                return false;
            }
        } else {
            BOOST_LOG_TRIVIAL(error) << "We have to read 9 values for IMG or 2 for MSK";
            BOOST_LOG_TRIVIAL(error) << "\t line : " << line;
            return false;
        }

        char* path = (char*)malloc(IMAGE_MAX_FILENAME_LENGTH);
        memset(path, 0, IMAGE_MAX_FILENAME_LENGTH);

        if (!strncmp(tmpPath, "?", 1)) {
            strcpy(path, images_root);
            strcpy(&(path[rootLength]), &(tmpPath[1]));
        } else {
            strcpy(path, tmpPath);
        }

        // On ajoute tout ça dans les vecteurs
        masks->push_back(is_mask);
        paths->push_back(path);
        srss->push_back(crs);
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
 * \param[out] sorted_input_images ensemble des images en entrée
 * \return code de retour, 0 si réussi, -1 sinon
 */
int load_images(FileImage** output_image, FileImage** output_mask, std::vector<FileImage*>* input_images) {
    std::vector<bool> masks;
    std::vector<char*> paths;
    std::vector<std::string> srss;
    std::vector<BoundingBox<double> > bboxes;
    std::vector<double> resxs;
    std::vector<double> resys;

    if (! load_configuration(&masks, &paths, &srss, &bboxes, &resxs, &resys)) {
        BOOST_LOG_TRIVIAL(error) << "Cannot load configuration file " << configuration_path;
        return -1;
    }

    // On doit avoir au moins deux lignes, trois si on a un masque de sortie
    if (masks.size() < 2 || (masks.size() == 2 && masks.back())) {
        BOOST_LOG_TRIVIAL(error) << "We have no input images in configuration file " << configuration_path;
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

    for (int i = first_input; i < masks.size(); i++) {
        input_count++;
        BOOST_LOG_TRIVIAL(debug) << "Input " << input_count;

        if (resxs.at(i) == 0. || resys.at(i) == 0.) {
            BOOST_LOG_TRIVIAL(error) << "Source image " << input_count << " is not valid (resolutions)";
            return -1;
        }

        CRS* crs = CrsBook::get_crs(srss.at(i));

        if (! crs->is_define()) {
            BOOST_LOG_TRIVIAL(error) << "Input CRS unknown: " << srss.at(i);
            return -1;
        } else {
            BOOST_LOG_TRIVIAL(debug) << crs->get_proj_code();
            bboxes.at(i).crs = crs->get_request_code();
        }

        if (! bboxes.at(i).is_in_crs_area(crs)) {
            BOOST_LOG_TRIVIAL(debug) << "Warning : the input image's (" << paths.at(i) << ") bbox is not included in the srs (" << srss.at(i) << ") definition extent";
            BOOST_LOG_TRIVIAL(debug) << bboxes.at(i).to_string() << " not included in " << crs->get_native_crs_definition_area().to_string();
        }

        FileImage* input_image = FileImage::create_to_read(paths.at(i), bboxes.at(i), resxs.at(i), resys.at(i));
        if (input_image == NULL) {
            BOOST_LOG_TRIVIAL(error) << "Impossible de creer une image a partir de " << paths.at(i);
            return -1;
        }
        input_image->set_crs(crs);
        free(paths.at(i));

        if (i + 1 < masks.size() && masks.at(i + 1)) {
            FileImage* input_mask = FileImage::create_to_read(paths.at(i + 1), bboxes.at(i), resxs.at(i), resys.at(i));
            if (input_mask == NULL) {
                BOOST_LOG_TRIVIAL(error) << "Impossible de creer un masque a partir de " << paths.at(i + 1);
                return -1;
            }
            input_mask->set_crs(crs);

            if (!input_image->set_mask(input_mask)) {
                BOOST_LOG_TRIVIAL(error) << "Cannot add mask to the input FileImage";
                return -1;
            }
            i++;
            free(paths.at(i));
        }

        input_images->push_back(input_image);

        /* On vérifie que le format des canaux est le même pour toutes les images en entrée :
         *     - sampleformat
         *     - samplesperpixel
         */

        if (! output_format_provided && input_count == 1) {
            /* On n'a pas précisé de format en sortie, on va donc utiliser celui des entrées
             * On veut donc avoir le même format pour toutes les entrées
             * On lit la première image en entrée, qui sert de référence
             * L'image en sortie sera à ce format
             */
            samplesperpixel = input_image->get_channels();
            sampleformat = input_image->get_sample_format();
        } else if (!output_format_provided) {
            // On doit avoir le même format pour tout le monde
            if (samplesperpixel != input_image->get_channels()) {
                BOOST_LOG_TRIVIAL(error) << "We don't provided output format, so all inputs have to own the same";
                BOOST_LOG_TRIVIAL(error) << "The first image and the " << input_count << " one don't have the same number of samples per pixel";
                BOOST_LOG_TRIVIAL(error) << samplesperpixel << " != " << input_image->get_channels();
            }
            if (sampleformat != input_image->get_sample_format()) {
                BOOST_LOG_TRIVIAL(error) << "We don't provided output format, so all inputs have to own the same";
                BOOST_LOG_TRIVIAL(error) << "The first image and the " << input_count << " one don't have the same sample format";
                BOOST_LOG_TRIVIAL(error) << sampleformat << " != " << input_image->get_sample_format();
            }
        }
    }

    if (input_images->size() == 0) {
        BOOST_LOG_TRIVIAL(error) << "Erreur lecture du fichier de parametres '" << configuration_path << "' : pas de données en entrée.";
        return -1;
    } else {
        BOOST_LOG_TRIVIAL(debug) << input_count << " image(s) en entrée";
    }

    /********************** LE STYLE : CRÉATION *************************/

    if (style_provided) {
        BOOST_LOG_TRIVIAL(debug) << "Load style";
        style = new Style(style_file);
        if ( ! style->is_ok() ) {
            BOOST_LOG_TRIVIAL(error) << style->get_error_message();
            BOOST_LOG_TRIVIAL(error) << "Cannot load style";
            return -1;
        }

        if ( ! style->handle(samplesperpixel) ) {
            BOOST_LOG_TRIVIAL(error) << "Cannot apply this style for this channels number";
            return -1;
        }

        // Le style peut modifier les caractéristiques
        samplesperpixel = style->get_channels(samplesperpixel);
        sampleformat = style->get_sample_format(sampleformat);
    }

    /********************** LA SORTIE : CRÉATION *************************/

    if (samplesperpixel == 1) {
        photometric = Photometric::GRAY;
    } else if (samplesperpixel == 2) {
        photometric = Photometric::GRAY;
    } else {
        photometric = Photometric::RGB;
    }

    CRS *output_crs = CrsBook::get_crs(srss.at(0));
    if (! output_crs->is_define()) {
        BOOST_LOG_TRIVIAL(error) << "Output CRS unknown: " << srss.at(0);
        return -1;
    }

    // Arrondi a la valeur entiere la plus proche
    int width = lround((bboxes.at(0).xmax - bboxes.at(0).xmin) / (resxs.at(0)));
    int height = lround((bboxes.at(0).ymax - bboxes.at(0).ymin) / (resys.at(0)));

    *output_image = FileImage::create_to_write(
        paths.at(0), bboxes.at(0), resxs.at(0), resys.at(0), width, height,
        samplesperpixel, sampleformat, photometric, compression);

    if (*output_image == NULL) {
        BOOST_LOG_TRIVIAL(error) << "Impossible de creer l'image " << paths.at(0);
        return -1;
    }

    (*output_image)->set_crs(output_crs);
    free(paths.at(0));

    if (first_input == 2) {
        *output_mask = FileImage::create_to_write(
            paths.at(1), bboxes.at(0), resxs.at(0), resys.at(0), width, height,
            1, SampleFormat::UINT8, Photometric::MASK, Compression::DEFLATE);

        if (*output_mask == NULL) {
            BOOST_LOG_TRIVIAL(error) << "Impossible de creer le masque " << paths.at(1);
            return -1;
        }

        (*output_mask)->set_crs(output_crs);
        free(paths.at(1));
    }

    if (debug_logger) (*output_image)->print();

    return 0;
}

int add_converters(std::vector<FileImage*> input_images) {
    if (! output_format_provided) {
        // On n'a pas précisé de format en sortie, donc toutes les images doivent avoir le même
        // Et la sortie a aussi ce format, donc pas besoin de convertisseur

        return 0;
    }

    for (std::vector<FileImage*>::iterator input_images_iterator = input_images.begin(); input_images_iterator < input_images.end(); input_images_iterator++) {
        if (!(*input_images_iterator)->add_converter(sampleformat, samplesperpixel)) {
            BOOST_LOG_TRIVIAL(error) << "Cannot add converter for an input image";
            (*input_images_iterator)->print();
            return -1;
        }

        if (debug_logger) (*input_images_iterator)->print();
    }

    return 0;
}

/**
 * \~french
 * \brief Trie les images sources en paquets d'images superposables
 * \details On réunit les images en paquets, dans lesquels :
 * \li toutes les images ont la même résolution, en X et en Y
 * \li toutes les images ont la même phase, en X et en Y
 *
 * On conserve cependant l'ordre originale des images, quitte à augmenter le nombre de paquets final.
 * Ce tri sert à simplifier le traitement des images et leur réechantillonnage.
 *
 * \~ \image html mergeNtiff_package.png \~french
 *
 * \param[in] input_images images en entrée
 * \param[out] sorted_input_images images en entrée, triées en paquets compatibles
 * \return code de retour, 0 si réussi, -1 sinon
 */
int sort_images(std::vector<FileImage*> input_images, std::vector<std::vector<Image*> >* sorted_input_images) {
    std::vector<Image*> current_pack;
    std::vector<FileImage*>::iterator current_input_images_iterator = input_images.begin();

    /* we create consistent images' vectors (X/Y resolution and X/Y phases)
     * Masks are moved in parallel with images
     */
    for (std::vector<FileImage*>::iterator input_images_iterator = input_images.begin(); input_images_iterator < input_images.end() - 1; input_images_iterator++) {
        if (!(*input_images_iterator)->compatible(*(input_images_iterator + 1))) {
            // two following images are not compatible, we split images' vector
            current_pack.assign(current_input_images_iterator, input_images_iterator + 1);
            current_input_images_iterator = input_images_iterator + 1;
            sorted_input_images->push_back(current_pack);
        }
    }

    // we don't forget to store last images in sorted_input_images
    // images
    current_pack.assign(current_input_images_iterator, input_images.end());
    sorted_input_images->push_back(current_pack);

    return 0;
}

/**
 * \~french
 * \brief Réechantillonne un paquet d'images compatibles
 * \details On crée l'objet ResampledImage correspondant au réechantillonnage du paquet d'images, afin de le rendre compatible avec l'image de sortie. On veut que l'emprise de l'image réechantillonnée ne dépasse ni de l'image de sortie, ni des images en entrée (sans prendre en compte les miroirs, données virtuelles).
 * \param[in] output_image image résultante de l'outil
 * \param[in] input_images paquet d'images compatibles, à réechantillonner
 * \param[in] resampled_image image réechantillonnée
 * \return VRAI si succès, FAUX sinon
 */
bool resample_images(FileImage* output_image, ExtendedCompoundImage* input_images, ResampledImage** resampled_image) {
    double resx_dst = output_image->get_resx(), resy_dst = output_image->get_resy();

    const Kernel& kernel = Kernel::get_instance(interpolation);

    // Ajout des miroirs
    // Valeurs utilisées pour déterminer la taille des miroirs en pixel (taille optimale en fonction du noyau utilisé)
    int mirror_size_x = ceil(kernel.size(resx_dst / input_images->get_resx())) + 1;
    int mirror_size_y = ceil(kernel.size(resy_dst / input_images->get_resy())) + 1;

    int mirror_size = std::max(mirror_size_x, mirror_size_y);

    BOOST_LOG_TRIVIAL(debug) << "\t Mirror's size : " << mirror_size;

    // On mémorise la bbox d'origine, sans les miroirs
    BoundingBox<double> real_bbox = input_images->get_bbox();

    if (! input_images->add_mirrors(mirror_size)) {
        BOOST_LOG_TRIVIAL(error) << "Unable to add mirrors";
        return false;
    }

    /* L'image reechantillonnee est limitee a l'intersection entre l'image de sortie et les images sources
     * (sans compter les miroirs)
     */
    double xmin_dst = std::max(real_bbox.xmin, output_image->get_xmin());
    double xmax_dst = std::min(real_bbox.xmax, output_image->get_xmax());
    double ymin_dst = std::max(real_bbox.ymin, output_image->get_ymin());
    double ymax_dst = std::min(real_bbox.ymax, output_image->get_ymax());

    BoundingBox<double> bbox_dst(xmin_dst, ymin_dst, xmax_dst, ymax_dst);

    /* Nous avons maintenant les limites de l'image réechantillonée. N'oublions pas que celle ci doit être compatible
     * avec l'image de sortie. Il faut donc modifier la bounding box afin qu'elle remplisse les conditions de compatibilité
     * (phases égales en x et en y).
     */
    bbox_dst.phase(output_image->get_bbox(), output_image->get_resx(), output_image->get_resy());

    // Dimension de l'image reechantillonnee
    int width_dst = int((bbox_dst.xmax - bbox_dst.xmin) / resx_dst + 0.5);
    int height_dst = int((bbox_dst.ymax - bbox_dst.ymin) / resy_dst + 0.5);

    if (width_dst <= 0 || height_dst <= 0) {
        BOOST_LOG_TRIVIAL(warning) << "A ResampledImage's dimension would have been null";
        return true;
    }

    // On réechantillonne le masque : TOUJOURS EN PPV, sans utilisation de masque pour l'interpolation
    ResampledImage* resampled_mask = new ResampledImage(input_images->Image::get_mask(), width_dst, height_dst, resx_dst, resy_dst, bbox_dst,
                                                Interpolation::NEAREST_NEIGHBOUR, false);

    // Reechantillonnage
    Image* input_to_resample = input_images;
    if (style_provided) {
        Image* styled_image = NULL;
        
        if (style->estompage_defined()) {
            styled_image = new EstompageImage (input_images, style->get_estompage());
        }
        else if (style->pente_defined()) {
            styled_image = new PenteImage (input_images, style->get_pente());
        }
        else if (style->aspect_defined()) {
            styled_image = new AspectImage (input_images, style->get_aspect()) ;           
        }

        if ( input_to_resample->get_channels() == 1 && ! ( style->get_palette()->is_empty() ) ) {
            if (styled_image != NULL) {
                input_to_resample = new PaletteImage ( styled_image , style->get_palette() );
            } else {
                input_to_resample = new PaletteImage ( input_images , style->get_palette() );
            }
        } else {
            if (styled_image != NULL) {
                input_to_resample = styled_image;
            }
        }
    }

    *resampled_image = new ResampledImage(input_to_resample, width_dst, height_dst, resx_dst, resy_dst, bbox_dst, interpolation, input_images->use_masks());

    if (!(*resampled_image)->set_mask(resampled_mask)) {
        BOOST_LOG_TRIVIAL(error) << "Cannot add mask to the ResampledImage";
        return false;
    }

    return true;
}

/**
 * \~french
 * \brief Reprojette un paquet d'images compatibles
 * \details On crée l'objet ReprojectedImage correspondant à la reprojection du paquet d'images, afin de le rendre compatible avec l'image de sortie. On veut que l'emprise de l'image réechantillonnée ne dépasse ni de l'image de sortie, ni des images en entrée (sans prendre en compte les miroirs, données virtuelles).
 *
 * L'image reprojetée doit être strictement incluse dans l'image source utilisée, c'est pourquoi on va artificiellement agrandir l'image source (avec du nodata) pour être sur de l'inclusion stricte.
 *
 * L'image reprojetée peut être nulle, dans le cas où l'image source ne recouvrait pas suffisemment l'image de sortie pour permettre le calcul d'une image (une dimensions de l'image reprojetée aurait été nulle).
 *
 * \param[in] output_image image résultante de l'outil
 * \param[in] input_images paquet d'images compatibles, à reprojeter
 * \param[in] reprojected_image image reprojetée
 * \return VRAI si succès, FAUX sinon
 */
bool reproject_images(FileImage* output_image, ExtendedCompoundImage* input_images, ReprojectedImage** reprojected_image) {
    // Calcul des paramètres de reprojection

    double resx_dst = output_image->get_resx(), resy_dst = output_image->get_resy();
    double resx_src = input_images->get_resx(), resy_src = input_images->get_resy();

    const Kernel& kernel = Kernel::get_instance(interpolation);

    /******** Conversion de la bbox source dans le srs de sortie ********/
    /******************* et calcul des ratios des résolutions ***********/

    BoundingBox<double> tmp_bbox = input_images->get_bbox().crop_to_crs_area(input_images->get_crs());

    int crop_width = ceil((tmp_bbox.xmax - tmp_bbox.xmin) / resx_src);
    int crop_height = ceil((tmp_bbox.ymax - tmp_bbox.ymin) / resy_src);

    if (!tmp_bbox.reproject(input_images->get_crs(), output_image->get_crs())) {
        BOOST_LOG_TRIVIAL(error) << "Erreur reprojection bbox src -> dst";
        return false;
    }

    /* On calcule les résolutions de l'image source "équivalente" dans le SRS de destination, pour pouvoir calculer le ratio
     * des résolutions pour la taille des miroirs */
    double resx_calc = (tmp_bbox.xmax - tmp_bbox.xmin) / double(crop_width);
    double resy_calc = (tmp_bbox.ymax - tmp_bbox.ymin) / double(crop_height);

    /******************** Image reprojetée : dimensions *****************/

    /* On fait particulièrement attention à ne considérer que la partie valide de la bbox finale
     * c'est à dire la partie incluse dans l'espace de définition du SRS
     * On va donc la "croper" */
    BoundingBox<double> croped_output_bbox = output_image->get_bbox().crop_to_crs_area(output_image->get_crs());

    BoundingBox<double> bbox_dst = croped_output_bbox.get_intersection(tmp_bbox);

    BOOST_LOG_TRIVIAL(debug) << "        BBOX dst (srs destination) : " << bbox_dst.to_string();

    /* Nous avons maintenant les limites de l'image reprojetée. N'oublions pas que celle ci doit être compatible
     * avec l'image de sortie. Il faut donc modifier la bounding box afin qu'elle remplisse les conditions de compatibilité
     * (phases égales en x et en y).
     */
    bbox_dst.phase(output_image->get_bbox(), output_image->get_resx(), output_image->get_resy());

    // Dimension de l'image reechantillonnee
    BOOST_LOG_TRIVIAL(debug) << "        Calculated destination width (float) : " << (bbox_dst.xmax - bbox_dst.xmin) / resx_dst;
    BOOST_LOG_TRIVIAL(debug) << "        Calculated destination height (float) : " << (bbox_dst.ymax - bbox_dst.ymin) / resy_dst;
    int width_dst = int((bbox_dst.xmax - bbox_dst.xmin) / resx_dst + 0.5);
    int height_dst = int((bbox_dst.ymax - bbox_dst.ymin) / resy_dst + 0.5);

    if (width_dst <= 0 || height_dst <= 0) {
        BOOST_LOG_TRIVIAL(warning) << "A ReprojectedImage's dimension would have been null";
        return true;
    }

    tmp_bbox = bbox_dst;

    if (! tmp_bbox.reproject(output_image->get_crs(), input_images->get_crs())) {
        BOOST_LOG_TRIVIAL(error) << "Erreur reprojection bbox dst en crs src";
        return false;
    }

    BOOST_LOG_TRIVIAL(debug) << "        BBOX dst (srs source) : " << tmp_bbox.to_string();
    BOOST_LOG_TRIVIAL(debug) << "        BBOX source : " << input_images->get_bbox().to_string();

    /************************ Ajout des miroirs *************************/

    double ratio_x = resx_dst / resx_calc;
    double ratio_y = resy_dst / resy_calc;

    // Ajout des miroirs
    int mirror_size_x = ceil(kernel.size(ratio_x)) + 1;
    int mirror_size_y = ceil(kernel.size(ratio_y)) + 1;

    int mirror_size = 2 * std::max(mirror_size_x, mirror_size_y);

    BOOST_LOG_TRIVIAL(debug) << "        Mirror's size : " << mirror_size;

    if (! input_images->add_mirrors(mirror_size)) {
        BOOST_LOG_TRIVIAL(error) << "Unable to add mirrors";
        return false;
    }

    BOOST_LOG_TRIVIAL(debug) << "        BBOX source avec miroir : " << input_images->get_bbox().to_string();

    /********************** Image source agrandie ***********************/

    /* L'image à reprojeter n'est pas intégralement contenue dans l'image source. Cela va poser des problèmes lors de l'interpolation :
     * ReprojectedImage va vouloir accéder à des coordonnées pixel négatives -> segmentation fault.
     * Pour éviter cela, on va agrandir artificiellemnt l'étendue de l'image source (avec du nodata) */
    if (! input_images->extend_bbox(tmp_bbox, mirror_size + 1)) {
        BOOST_LOG_TRIVIAL(error) << "Unable to extend the source image extent for the reprojection";
        return false;
    }
    BOOST_LOG_TRIVIAL(debug) << "        BBOX source agrandie : " << input_images->get_bbox().to_string();

    /********************** Grille de reprojection **********************/

    Grid* grid = new Grid(width_dst, height_dst, bbox_dst);

    if (!(grid->reproject(output_image->get_crs(), input_images->get_crs()))) {
        BOOST_LOG_TRIVIAL(error) << "Bbox image invalide";
        return false;
    }

    grid->affine_transform(1. / resx_src, -input_images->get_bbox().xmin / resx_src - 0.5,
                           -1. / resy_src, input_images->get_bbox().ymax / resy_src - 0.5);


    /********************** Application du style **********************/
    Image* input_to_reproject = input_images;
    if (style_provided) {
        Image* styled_image = NULL;

        if (style->estompage_defined()) {
            styled_image = new EstompageImage (input_images, style->get_estompage());
        }
        else if (style->pente_defined()) {
            styled_image = new PenteImage (input_images, style->get_pente());
        }
        else if (style->aspect_defined()) {
            styled_image = new AspectImage (input_images, style->get_aspect()) ;           
        }

        if ( input_to_reproject->get_channels() == 1 && ! ( style->get_palette()->is_empty() ) ) {
            if (styled_image != NULL) {
                input_to_reproject = new PaletteImage ( styled_image , style->get_palette() );
            } else {
                input_to_reproject = new PaletteImage ( input_images , style->get_palette() );
            }
        } else {
            if (styled_image != NULL) {
                input_to_reproject = styled_image;
            }
        }

        input_to_reproject->set_crs(input_images->get_crs());
    }

    /*************************** Image reprojetée ***********************/

    // On  reprojete le masque : TOUJOURS EN PPV, sans utilisation de masque pour l'interpolation
    ReprojectedImage* reprojected_mask = new ReprojectedImage(
        input_images->Image::get_mask(), bbox_dst, resx_dst, resy_dst, grid,
        Interpolation::NEAREST_NEIGHBOUR, false
    );
    reprojected_mask->set_crs(output_image->get_crs());

    // Reprojection de l'image

    *reprojected_image = new ReprojectedImage(input_to_reproject, bbox_dst, resx_dst, resy_dst, grid, interpolation, input_images->use_masks());
    (*reprojected_image)->set_crs(output_image->get_crs());

    if (!(*reprojected_image)->set_mask(reprojected_mask)) {
        BOOST_LOG_TRIVIAL(error) << "Cannot add mask to the ReprojectedImage";
        return false;
    }

    return true;
}

/**
 * \~french \brief Traite chaque paquet d'images en entrée
 * \~english \brief Treat each input images pack
 * \~french \details On a préalablement trié les images par compatibilité. Pour chaque paquet, on va créer un objet de la classe ExtendedCompoundImage. Ce dernier est à considérer comme une image simple.
 * Cette image peut être :
 * \li superposable avec l'image de sortie. Elle est directement ajoutée à une liste d'image.
 * \li non superposable avec l'image de sortie mais dans le même système de coordonnées. On va alors la réechantillonner, en utilisant la classe ResampledImage. C'est l'image réechantillonnée que l'on ajoute à la liste d'image.
 * \li non superposable avec l'image de sortie et dans un système de coordonnées différent. On va alors la reprojeter, en utilisant la classe ReprojectedImage. C'est l'image reprojetée que l'on ajoute à la liste d'image.
 *
 * \~ \image html mergeNtiff_composition.png \~french
 *
 * On obtient donc une liste d'images superposables avec celle de sortie, que l'on va réunir sous un objet de la classe ExtendedCompoundImage, qui sera la source unique utilisée pour écrire l'image de sortie.
 *
 * \~ \image html mergeNtiff_decoupe.png \~french
 *
 * Les masques sont gérés en toile de fond, en étant attachés à chacune des images manipulées.
 * \param[in] output_image image de sortie
 * \param[in] input_images paquets d'images en entrée
 * \param[out] merged_image paquet d'images superposable avec l'image de sortie
 * \return 0 en cas de succès, -1 en cas d'erreur
 */
int merge_images(FileImage *output_image,                          // Sortie
                   std::vector<std::vector<Image *>> &sorted_input_images, // Entrée
                   ExtendedCompoundImage **merged_image)              // Résultat du merge
{
    std::vector<Image*> stackable_images;

    // Les données en entrée sont remplies :
    // - Avec le nodata fourni si pas de style
    // - Avec le nodata attendu en entrée du style fourni
    int* nd = nodata;
    if (style_provided) {
        nd = style->get_input_nodata_value(nd);
    }

    for (unsigned int i = 0; i < sorted_input_images.size(); i++) {
        BOOST_LOG_TRIVIAL(debug) << "Pack " << i << " : " << sorted_input_images.at(i).size() << " image(s)";
        // Mise en superposition du paquet d'images en 2 etapes

        // Etape 1 : Creation d'une image composite (avec potentiellement une seule image)

        ExtendedCompoundImage* stackable_image = ExtendedCompoundImage::create(sorted_input_images.at(i), nd, 0);
        if (stackable_image == NULL) {
            BOOST_LOG_TRIVIAL(error) << "Impossible d'assembler les images";
            return -1;
        }
        stackable_image->set_crs(sorted_input_images.at(i).at(0)->get_crs());

        ExtendedCompoundMask* stackable_mask = new ExtendedCompoundMask(stackable_image);

        stackable_mask->set_crs(sorted_input_images.at(i).at(0)->get_crs());
        if (! stackable_image->set_mask(stackable_mask)) {
            BOOST_LOG_TRIVIAL(error) << "Cannot add mask to the Image's pack " << i;
            return -1;
        }

        if (output_image->compatible(stackable_image)) {
            BOOST_LOG_TRIVIAL(debug) << "\t is compatible";
            /* les images sources et finale ont la meme resolution et la meme phase
             * on n'aura donc pas besoin de reechantillonnage.*/
            if (style_provided && ! (i == 0 && background_provided)) {
                // Un style est fourni et nous ne sommes pas dans le cas de la première entrée qui est une image de fond
                Image* styled_image = NULL;
                
                if (style->estompage_defined()) {
                    styled_image = new EstompageImage (stackable_image, style->get_estompage());
                }
                else if (style->pente_defined()) {
                    styled_image = new PenteImage (stackable_image, style->get_pente());
                }
                else if (style->aspect_defined()) {
                    styled_image = new AspectImage (stackable_image, style->get_aspect()) ;           
                }

                if ( stackable_image->get_channels() == 1 && ! ( style->get_palette()->is_empty() ) ) {
                    if (styled_image != NULL) {
                        stackable_images.push_back(new PaletteImage ( styled_image , style->get_palette() ));
                    } else {
                        stackable_images.push_back(new PaletteImage ( stackable_image , style->get_palette() ));
                    }
                } else {
                    stackable_images.push_back(styled_image);
                }
            } else {
                stackable_images.push_back(stackable_image);
            }

        } else if (stackable_image->get_crs()->cmp_request_code(output_image->get_crs()->get_request_code())) {
            BOOST_LOG_TRIVIAL(debug) << "\t need a resampling";

            ResampledImage* resampled_image = NULL;

            if (!resample_images(output_image, stackable_image, &resampled_image)) {
                BOOST_LOG_TRIVIAL(error) << "Cannot resample images' pack";
                return -1;
            }

            if (resampled_image == NULL) {
                BOOST_LOG_TRIVIAL(warning) << "No resampled image to add";
            } else {
                stackable_images.push_back(resampled_image);
            }

        } else {
            BOOST_LOG_TRIVIAL(debug) << "\t need a reprojection";

            ReprojectedImage* reprojected_image = NULL;

            if (!reproject_images(output_image, stackable_image, &reprojected_image)) {
                BOOST_LOG_TRIVIAL(error) << "Cannot reproject images' pack";
                return -1;
            }

            if (reprojected_image == NULL) {
                BOOST_LOG_TRIVIAL(warning) << "No reprojected image to add";
            } else {
                stackable_images.push_back(reprojected_image);
            }
        }
    }


    // Les données en sortie sont remplies :
    // - Avec le nodata fourni si pas de style
    // - Avec le nodata de sortie du style fourni
    nd = nodata;
    if (style_provided) {
        nd = style->get_output_nodata_value(nd);
    }

    for (int i = 0; i < output_image->get_channels(); i++) {
        BOOST_LOG_TRIVIAL(debug) << "output nodata [" << i << "] = " << nd[i];
    }

    // Assemblage des paquets et decoupage aux dimensions de l image de sortie
    *merged_image = ExtendedCompoundImage::create(
        output_image->get_width(), output_image->get_height(), output_image->get_channels(), output_image->get_bbox(),
        stackable_images, nd, 0);

    if (*merged_image == NULL) {
        for (int i = 0; i < stackable_images.size(); i++) delete stackable_images.at(i);
        BOOST_LOG_TRIVIAL(error) << "Cannot create final compounded image.";
        return -1;
    }

    // Masque
    ExtendedCompoundMask* merged_mask = new ExtendedCompoundMask(*merged_image);

    if (!(*merged_image)->set_mask(merged_mask)) {
        BOOST_LOG_TRIVIAL(error) << "Cannot add mask to the main Extended Compound Image";
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
int main(int argc, char** argv) {
    FileImage* output_image = NULL;
    FileImage* output_mask = NULL;
    std::vector<FileImage*> input_images;
    std::vector<std::vector<Image*> > sorted_input_images;
    ExtendedCompoundImage* merged_image = NULL;

    /* Initialisation des Loggers */
    boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::info);
    logging::add_common_attributes();
    boost::log::register_simple_formatter_factory<boost::log::trivial::severity_level, char>("Severity");
    logging::add_console_log(
        std::cout,
        keywords::format = "%Severity%\t%Message%"
    );

    // Lecture des parametres de la ligne de commande
    if (parse_command_line(argc, argv) < 0) {
        error("Echec lecture ligne de commande", -1);
    }

    // On sait maintenant si on doit activer le niveau de log DEBUG
    if (debug_logger) {
        boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::debug);
    }

    // On regarde si on a tout précisé en sortie, pour voir si des conversions sont possibles
    if (sampleformat != SampleFormat::UNKNOWN && samplesperpixel != 0) {
        output_format_provided = true;
    }
    if (output_format_provided && style_provided) {
        error("Impossible d'appliquer un style et une conversion à la volée", -1);
    }

    if (! style_provided && ! nodata_provided) {
        error("Préciser une valeur de nodata est obligatoire sans style", -1);
    }

    BOOST_LOG_TRIVIAL(debug) << "Load";
    // Chargement des images
    if (load_images(&output_image, &output_mask, &input_images) < 0) {
        if (merged_image) delete merged_image;
        if (output_image) delete output_image;
        if (output_mask) delete output_mask;
        CrsBook::clean_crss();
        ProjPool::clean_projs();
        proj_cleanup();
        error("Echec chargement des images", -1);
    }

    BOOST_LOG_TRIVIAL(debug) << "Add converters";
    // Ajout des modules de conversion aux images en entrée
    if (add_converters(input_images) < 0) {
        if (merged_image) delete merged_image;
        if (output_image) delete output_image;
        if (output_mask) delete output_mask;
        CrsBook::clean_crss();
        ProjPool::clean_projs();
        proj_cleanup();
        error("Echec ajout des convertisseurs", -1);
    }

    // Maintenant que l'on a la valeur de samplesperpixel, on peut lire le nodata fourni
    // Conversion string->int[] du paramètre nodata

    if (nodata_provided) {
        BOOST_LOG_TRIVIAL(debug) << "Nodata interpretation";
        
        nodata = new int[samplesperpixel];

        char* char_iterator = strtok(strnodata, ",");
        if (char_iterator == NULL) {
            if (merged_image) delete merged_image;
            if (output_image) delete output_image;
            if (output_mask) delete output_mask;
            CrsBook::clean_crss();
            ProjPool::clean_projs();
            proj_cleanup();
            error("Error with option -n : a value for nodata is missing", -1);
        }
        nodata[0] = atoi(char_iterator);

        for (int i = 1; i < samplesperpixel; i++) {
            char_iterator = strtok(NULL, ",");
            if (char_iterator == NULL) {
                if (merged_image) delete merged_image;
                if (output_image) delete output_image;
                if (output_mask) delete output_mask;
                CrsBook::clean_crss();
                ProjPool::clean_projs();
                proj_cleanup();
                error("Error with option -n : one value per sample(" + std::to_string(samplesperpixel) + "), separate with comma", -1);
            }
            nodata[i] = atoi(char_iterator);
        }
    }

    BOOST_LOG_TRIVIAL(debug) << "Sort";
    // Tri des images
    if (sort_images(input_images, &sorted_input_images) < 0) {
        if (merged_image) delete merged_image;
        if (output_image) delete output_image;
        if (output_mask) delete output_mask;
        delete[] nodata;
        CrsBook::clean_crss();
        ProjPool::clean_projs();
        proj_cleanup();
        error("Echec tri des images", -1);
    }

    BOOST_LOG_TRIVIAL(debug) << "Merge";
    // Fusion des paquets d images
    if (merge_images(output_image, sorted_input_images, &merged_image) < 0) {
        if (merged_image) delete merged_image;
        if (output_image) delete output_image;
        if (output_mask) delete output_mask;
        delete[] nodata;
        CrsBook::clean_crss();
        ProjPool::clean_projs();
        proj_cleanup();
        error("Echec fusion des paquets d images", -1);
    }

    BOOST_LOG_TRIVIAL(debug) << "Save image";
    // Enregistrement de l'image fusionnée
    if (output_image->write_image(merged_image) < 0) {
        if (merged_image) delete merged_image;
        if (output_image) delete output_image;
        if (output_mask) delete output_mask;
        delete[] nodata;
        CrsBook::clean_crss();
        ProjPool::clean_projs();
        proj_cleanup();
        error("Echec enregistrement de l image finale", -1);
    }

    if (output_mask != NULL) {
        BOOST_LOG_TRIVIAL(debug) << "Save mask";
        // Enregistrement du masque fusionné, si demandé
        if (output_mask->write_image(merged_image->Image::get_mask()) < 0) {
            if (merged_image) delete merged_image;
            if (output_image) delete output_image;
            if (output_mask) delete output_mask;
            delete[] nodata;
            CrsBook::clean_crss();
            ProjPool::clean_projs();
            proj_cleanup();
            error("Echec enregistrement du masque final", -1);
        }
    }

    BOOST_LOG_TRIVIAL(debug) << "Clean";
    // Nettoyage
    if (style_provided) {
        delete style;
    }
    if (nodata_provided) {
        delete[] nodata;
    }
    delete merged_image;
    delete output_image;
    delete output_mask;
    CrsBook::clean_crss();
    ProjPool::clean_projs();
    proj_cleanup();
    StoragePool::clean_storages();
    CurlPool::clean_curls();
    curl_global_cleanup();

    return 0;
}