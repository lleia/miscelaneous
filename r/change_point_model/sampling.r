# length of sequence
N <- 100
# parameters
n <- 60
lm1 <- 10
lm2 <- 15

# samples 
pl <- rbinom(n,20,0.5)
pr <- rbinom(N-n,60,0.5)
#pl <- rep(5,60)
#pr <- rep(12,40)
pa <- c(pl,pr)

ITERS <- 10000
# initial values
en <- 50
elm1 <- 50
elm2 <- 70 

# sampling results
en_list <- rep(0,ITERS)
elm1_list <- rep(0.0,ITERS)
elm2_list <- rep(0.0,ITERS)

# multinomials to sample n
mult_prob <- rep(0.0, N)

for (id in 1:ITERS) {

	# calculate the multinomials to sampling n
	tslm1 <- pa[1] * log(elm1) 
	tslm2 <- 0
	for (mid in 2:N) {
		tslm2 <- tslm2 + pa[mid]
	}
	tslm2 <- tslm2 * log(elm2)

	for (mid in 1:N) {
		tmp <- tslm1 + tslm2 - mid*elm1 - (N-mid)*elm2
		#mult_prob[mid] <- exp(tslm1 + tslm2 - en*elm1 - (N-en)*elm2)
		mult_prob[mid] <- 1 + tmp^4 + tmp^8 + tmp^16 + tmp^32;
		if (mid > 1 && mid < N) {
			tslm1 <- tslm1 + pa[mid+1]*log(elm1)
			tslm2 <- tslm2 - pa[mid+1]*log(elm2)
		}
	}
	
	# normalize
	sum_prob <- sum(mult_prob)
	mult_prob <- mult_prob/sum_prob

	# sampling n
	en_ret <- rmultinom(1,1,mult_prob)
	for (mid in 1:N) {
		if (en_ret[mid,1] == 1) {
			en <- mid
			break
		}
	}
	en_list[id] <- en
	
	lsum <- 0
	mid <- 1
	while (mid <= en) {
		lsum <- lsum + pa[mid]
		mid <- mid + 1
	}

	rsum <- 0
	mid <- en + 1
	while (mid <= N){
		rsum <- rsum + pa[mid]
		mid <- mid + 1
	}

	# sampling lm1
	elm1 <- rgamma(1,2+lsum,1+en)	
	elm1_list[id] <- elm1
	# sampling lm2
	elm2 <- rgamma(1,2+rsum,1+N-en)
	elm2_list[id] <- elm2
}
