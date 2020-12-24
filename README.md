*client 1:
    - Au premier segment reçu : "I need something bigger"
        => fenêtre normal et suppression de l'envoie de la taille du fichier côté server

    - Rcv a duplicate ACK puis freeze
        => start au numSeq 0 (et pas 10)

    (i) Première idée de comportement : renvoie les ACK dans l'ordre inverse/aléatoire + des timeout

*Problème :
    - sstresh : trop faible
    - flightsize : trop faible
    - window : trop faible dépasse jamais 2 ou 3