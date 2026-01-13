# Changelog
Tous les changements sont consignés dans ce fichier.

Le format est basé sur [Keep a Changelog](https://keepachangelog.com/) et ce projet respecte le [Semantic Versioning](https://semver.org/).

## [Unreleased]
### Added
- Possibilité de définir un timeout via la variable d'environnement `ROK4_NETWORK_TIMEOUT` (valeur à fournir en seconde) pour les intéraction avec le stockage Swift ou S3

### Changed
- Refonte du CHANGELOG au format [Keep a Changelog](https://keepachangelog.com/)
- Changement des imports de Cache aux imports spécifiques des classes suite à la séparation du fichier Cache dans core-cpp. 
- Merge4tiff utilise désormais les classes Image, dont la nouvelle SubsampledImage, pour faire le calcul raster
### Deprecated
### Removed
### Fixed
### Security

## [5.0.0] - 2025-06-13
### Changed
- Le format des canaux contient la taille en bits
- Passage en snake case

### Removed
- Suppression de l'option crop dans work2cache

### Fixed
- Correction de l'utilisation d'un style "identité"

## [4.2.0] - 2024-03-21
### Added
- Stockage objet (S3, Swift et Ceph)
    * Possibilité de définir un nombre de tentatives pour les lectures (1 par défaut) : variable d'environnement `ROK4_OBJECT_READ_ATTEMPTS`
    * Possibilité de définir un nombre de tentatives pour les écritures (1 par défaut) : variable d'environnement `ROK4_OBJECT_WRITE_ATTEMPTS`
    * Possibilité de définir un temps d'attente, en secondes, entre les tentatives (5 par défaut) : variable d'environnement `ROK4_OBJECT_ATTEMPTS_WAIT`

## [4.1.5] - 2023-08-30
### Fixed
- Outil `mergeNtiff` : correction du cas mergeNtiff + image de fond + style. L'image de fond provient d'une pyramide, donc est déjà au format cible. Il ne faut pas lui appliquer de style.

## [4.1.4] - 2023-03-14
### Changed
- Prise en compte du nouveau nommage dans l'utilisation de l'annuaire de contexte de stockage
- Compilation avec core-cpp en librairie dynamique

### Fixed
- Outil `manageNodata` : une erreur lors de la lecture initiale (et complète) de l'image de données en entrée fait sortir en erreur la commande
- Outil `mergeNtiff` : gestion correcte d'un style de pente pur (sans palette), dans le cas d'images en entrée phasée avec la sortie (pas de réechantillonnage ou reprojection)
- Include de la librairie d'annuaire (stockage, proj...)

## [4.0.0] - 2022-06-26
Le projet ROK4 a été totalement refondu, dans son organisation et sa mise à disposition. Les composants sont désormais disponibles dans des releases sur GitHub au format debian.

Cette release contient les outils de génération des pyramides de données, permettant les reprojections, le sous echantillonnage, ou encore la mise au format final des données.

### Added
- L'outil mergeNtiff peut prendre en compte un style à appliquer aux données, avant l'éventuelle reprojection

### Changed
- Les chemins des dalles finales sont fournis dans un format précisant le type de stockage : `(file|ceph|s3|swift)://<chemin vers le fichier ou l'objet>`. Dans le cas du stockage objet, le chemin est de la forme `<nom du contenant>/<nom de l'objet>`
- Passage de la librairie PROJ à la version 6

[Unreleased]: https://github.com/rok4/generation/compare/v5.0.0...HEAD
[5.0.0]: https://github.com/rok4/generation/compare/v4.2.0...v5.0.0
[4.2.0]: https://github.com/rok4/generation/compare/v4.1.5...v4.2.0
[4.1.5]: https://github.com/rok4/generation/compare/v4.1.4...v4.1.5
[4.1.4]: https://github.com/rok4/generation/compare/v4.0.0...v4.1.4
[4.0.0]: https://github.com/rok4/generation/releases/tag/v4.0.0
