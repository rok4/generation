## 4.1.5

### [Fixed]

* Outil `mergeNtiff` : correction du cas mergeNtiff + image de fond + style. L'image de fond provient d'une pyramide, donc est déjà au format cible. Il ne faut pas lui appliquer de style.

## 4.1.4

### [Fixed]

* Outil `manageNodata` : une erreur lors de la lecture initiale (et complète) de l'image de données en entrée fait sortir en erreur la commande
* Outil `mergeNtiff` : gestion correcte d'un style de pente pur (sans palette), dans le cas d'images en entrée phasée avec la sortie (pas de réechantillonnage ou reprojection)
* Include de la librairie d'annuaire (stockage, proj...)

### [Changed]

* Prise en compte du nouveau nommage dans l'utilisation de l'annuaire de contexte de stockage
* Compilation avec core-cpp en librairie dynamique

## 4.0.0

Le projet ROK4 a été totalement refondu, dans son organisation et sa mise à disposition. Les composants sont désormais disponibles dans des releases sur GitHub au format debian.

Cette release contient les outils de génération des pyramides de données, permettant les reprojections, le sous echantillonnage, ou encore la mise au format final des données.

### [Added]

* L'outil mergeNtiff peut prendre en compte un style à appliquer aux données, avant l'éventuelle reprojection

### [Changed]

* Les chemins des dalles finales sont fournis dans un format précisant le type de stockage : `(file|ceph|s3|swift)://<chemin vers le fichier ou l'objet>`. Dans le cas du stockage objet, le chemin est de la forme `<nom du contenant>/<nom de l'objet>`
* Passage de la librairie PROJ à la version 6 