/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef KIND_H
#define KIND_H 1

#include <stdlib.h>
#include <stdbool.h>
#include "utility.h"
#include "type.h"

struct NecroBase;

/*
    Uniqueness Types?
        * Idris: http://docs.idris-lang.org/en/latest/reference/uniqueness-types.html
        * "Uniqueness Types, Simplified": http://www.edsko.net/pubs/ifl07-paper.pdf

    Look into new syntax for adding kind signatures to type variables in type signatures (similar to how Idris):
        * (a :: UniqueType) -> (b :: AnyType) -> f a

    Need syntax for declaraing unique data types:
        * data UArray (n :: Nat) a :: UniqueType = UArray a

    * Rule 1: Function type is UniqueType if either a or b is a UniqueType
    * Rule 2: If a data type constructors contains a UniqueType the data type itself must be a UniqueType. (But you can declare a UniqueType which does not contain any UniqueTypes.)
    * Rule 3: Recursive values? How are they handled?


    - Idea 2, Alms influenced Affine Kinds (but without subtyping):
        * "Practical Affine Types": http://users.eecs.northwestern.edu/~jesse/pubs/alms/tovpucella-alms.pdf
        * "Practical Affine Types": https://www.researchgate.net/publication/216817423_Practical_Affine_Types/download

    - Type Constructors (including the function arrow (->)) are dependently kinded based on their input values, i.e.:
        * data Maybe a = Just a | Nothing
        * Maybe has kind:
            forall (a : k). k -> k
        * data Either a b = Left a | Right b
        * Either has kind:
            forall (a : k1) (b : k2). k1 -> k2 -> k1 | k2

sinOsc :: Audio -> Audio
sinOsc freq = result
  where
    (result, sinOscState ~ default) = accumulateNewAudio1 updateSinOscState freq sinOscState

updateSinOscState:: Double -> *SinOscState -> (Double, *SinOscState)
accumulateNewAudio1 :: (Double -> *s -> (Double, *s)) -> Audio -> *s -> (Array, *s)

    * Idea 3, Clean style Uniqueness Types, but simplified:
        - "Equality based Uniqueness Types": https://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.160.5690&rep=rep1&type=pdf

// World based IO
outputAudio :: Audio -> Int -> *World -> *World
outputFrame :: Frame -> *World -> *World
recordAudio :: Audio -> Text -> *World -> *World
main        :: *World -> *World
main = outAudio synths 1 |> outputFrame scene |> recordAudio synths fileName

// Type state based IO
openFile  :: Text  -> *File
writeFile :: Text -> *File -> *File
closeFile :: *File -> *()

openAudioChan  :: *AudioChan
writeAudioChan :: Audio -> *AudioChan -> *AudioChan
closeAudioChan :: *AudioChan -> *()

*/

NecroType*             necro_kind_fresh_kind_var(NecroPagedArena* arena, struct NecroBase* base);
void                   necro_kind_init_kinds(struct NecroBase* base, struct NecroScopedSymTable* scoped_symtable, NecroIntern* intern);
NecroResult(NecroType) necro_kind_unify_with_info(NecroType* kind1, NecroType* kind2, NecroScope* scope, NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroType) necro_kind_unify(NecroType* kind1, NecroType* kind2, NecroScope* scope);
NecroResult(NecroType) necro_kind_infer(NecroPagedArena* arena, struct NecroBase* base, NecroType* type, NecroSourceLoc source_loc, NecroSourceLoc end_loc);
void                   necro_kind_default_type_kinds(NecroPagedArena* arena, struct NecroBase* base, NecroType* type);
NecroResult(void)      necro_kind_infer_default_unify_with_star(NecroPagedArena* arena, struct NecroBase* base, NecroType* type, NecroScope* scope, NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(void)      necro_kind_infer_default_unify_with_kind(NecroPagedArena* arena, struct NecroBase* base, NecroType* type, NecroType* kind, NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(void)      necro_kind_infer_default(NecroPagedArena* arena, struct NecroBase* base, NecroType* type, NecroSourceLoc source_loc, NecroSourceLoc end_loc);

#endif // KIND_H