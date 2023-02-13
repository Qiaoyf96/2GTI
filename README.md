# Optimizing Guided Traversal for Fast Learned Sparse Retrieval

This repo provides the 2GTI implementation proposed in the paper "Optimizing Guided Traversal for Fast Learned Sparse Retrieval". The implementation is based on the [PISA](https://github.com/pisa-engine/pisa) search engine.

The data for SPLADE++ can be found [here](https://drive.google.com/drive/folders/12OtiCgdDLE0SUpz_ALFfL4CoT6iaLSIl?usp=share_link).

## Build

```
mkdir build
cd build
cmake ..
make
```

## Build Index
```
cd bin
./create_wand_data -c $DATA/2GTI -o 2GTI.wand -s gt -b 512
./compress_inverted_index -c $DATA/2GTI -o 2GTI.index -e block_simdbp
```

## Evaluate
```
# calculate the relevance in parallel
./evaluate_queries -e block_simdbp -i 2GTI.index -w 2GTI.wand -q $DATA/msmarco_dev.queries -k 10 -a maxscore --weighted -s gt --documents $DATA/msmarco.lex --alpha 1 --beta 0.3 --gamma 0.05 > 2GTI.trec

# measure the latency in single thread
./queries -e block_simdbp -i 2GTI.index -w 2GTI.wand -q $DATA/msmarco_dev.queries -k 10 -a maxscore --weighted -s gt --alpha 1 --beta 0.3 --gamma 0.05
```

## Reference

```
@inproceedings{2GTI2023,
  author    = {Yifan Qiao, Yingrui Yang, Haixin Lin, Tao Yang},
  title     = {Optimizing Guided Traversal for Fast Learned Sparse Retrieval},
  booktitle = {Proceedings of the ACM Web Conference 2023 (WWW ’23), May 1–5, 2023, Austin, TX, USA},
  year      = {2023},
  url       = {https://doi.org/10.1145/3543507.3583497}
}
```
