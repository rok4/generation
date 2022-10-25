# Outils de génération ROK4

## Summary

Le projet ROK4 a été totalement refondu, dans son organisation et sa mise à disposition. Les composants sont désormais disponibles dans des releases sur GitHub au format debian.

Cette release contient les outils de génération des pyramides de données, permettant les reprojections, le sous echantillonnage, ou encore la mise au format final des données.

## Changelog

### [Fixed]

* Outil `manageNodata` : une erreur lors de la lecture initiale (et complète) de l'image de données en entrée fait sortir en erreur la commande
* Outil `mergeNtiff` : gestion correcte d'un style de pente pur (sans palette), dans le cas d'images en entrée phasée avec la sortie (pas de réechantillonnage ou reprojection)

<!-- 
### [Added]

### [Changed]

### [Deprecated]

### [Removed]

### [Fixed]

### [Security] 
-->