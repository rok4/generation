## Summary

Corrections de la gestion du style dans mergeNtiff et ajout d'includes manquants

## Changelog

### [Fixed]

* Outil `manageNodata` : une erreur lors de la lecture initiale (et complète) de l'image de données en entrée fait sortir en erreur la commande
* Outil `mergeNtiff` : gestion correcte d'un style de pente pur (sans palette), dans le cas d'images en entrée phasée avec la sortie (pas de réechantillonnage ou reprojection)
* Include de la librairie d'annuaire (stockage, proj...)

### [Changed]

* Prise en compte du nouveau nommage dans l'utilisation de l'annuaire de contexte de stockage
* Compilation avec core-cpp en librairie dynamique

<!-- 
### [Added]

### [Changed]

### [Deprecated]

### [Removed]

### [Fixed]

### [Security] 
-->