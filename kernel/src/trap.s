.global trap_vector
trap_vector:
    j trap_vector // infinite loop on fault
