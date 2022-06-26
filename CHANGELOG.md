# Outils de génération ROK4

## Summary

Le projet ROK4 a été totalement refondu, dans son organisation et sa mise à disposition. Les composants sont désormais disponibles dans des releases sur GitHub au format debian.

Cette release contient les outils de génération des pyramides de données, permettant les reprojections, le sous echantillonnage, ou encore la mise au format final des données.

## Changelog

### [Added]

* L'outil mergeNtiff peut prendre en compte un style à appliquer aux données, avant l'éventuelle reprojection

### [Changed]

* Les chemins des dalles finales sont fournis dans un format précisant le type de stockage : `(file|ceph|s3|swift)://<chemin vers le fichier ou l'objet>`. Dans le cas du stockage objet, le chemin est de la forme `<nom du contenant>/<nom de l'objet>`
* Passage de la librairie PROJ à la version 6 

<!-- 
### [Added]

### [Changed]

### [Deprecated]

### [Removed]

### [Fixed]

### [Security] 
-->