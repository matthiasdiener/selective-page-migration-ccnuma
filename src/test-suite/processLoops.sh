awk '{
        if ($2 == 0) {
            numLinesT0++;
            totalT0+=$4;
        } else if($2 == 1) {
            numLinesT1++;
            totalT1+=$4;
        } else if($2 == 2) {
            numLinesT2++;
            totalT2+=$4;
        }
        
        numLines++; 
        total+=$4
     } END {
        if (numLinesT0 > 0) {
            printf "TripCount Accuracy (Interval Loops)	%f\n", totalT0/numLinesT0;
        } else if (numLinesT1 > 0) {
            printf "TripCount Accuracy (Equality Loops)	%f\n", totalT1/numLinesT1;
        } else if (numLinesT2 > 0) {
            printf "TripCount Accuracy (Other Loops)	%f\n", totalT2/numLinesT2;
        }   
        if (numLines > 0) {
            printf "TripCount Accuracy (All Loops)	%f\n", total/numLines;
        }              
     }' $1
