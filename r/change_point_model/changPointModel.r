# 
# llei@qq.com
# 2014-8-21
# A simple gibbs sampler for the change point problem.
#    http://www.bcs.rochester.edu/people/robbie/jacobslab/cheat_sheet/GibbsSampling.pdf
# I've also writen a technical note:
#    https://github.com/lleia/tech.notes/blob/master/simple_gibbs_sampling/gibbs.pdf    
#

# length of sequence
lengthSeq <- 100
changePoint <- 60
leftExpection <- 10
rightExpection <- 15

# generate samples 
leftSeq <- rbinom(changePoint,leftExpection*2,0.5)
rightSeq <- rbinom(lengthSeq-changePoint,rightExpection*2,0.5)
dataSeq <- c(leftSeq,rightSeq)

# training parameters
maxIterationCount <- 10000
gammaShape <- 2
gammaRate <- 1 

# traning variables 
estimateChangePoint <- 50
#estimateLeftExpection <- 50 
#estimateRightExpection <- 70 
estimateLeftExpection <- rgamma(1,gammaShape,rate=gammaRate)
estimateRightExpection <- rgamma(1,gammaShape,rate=gammaRate)

estimateChangePointList <- rep(0,maxIterationCount)
estimateLeftExpectionList <- rep(0,maxIterationCount)
estimateRightExpectionList <- rep(0,maxIterationCount)

probList <- c(0.0,lengthSeq)

for (id in 1:maxIterationCount) {
	# calculate the multinomials to sample change point 
	leftSum <- dataSeq[1] * log(estimateLeftExpection)
	rightSum <- sum(dataSeq[2:lengthSeq]) * log(estimateRightExpection)
	for (mid in 1:lengthSeq) {
		probList[mid] <- leftSum + rightSum - mid*estimateLeftExpection - (lengthSeq-mid)*estimateRightExpection
		if (mid > 1 && mid < lengthSeq) {
			leftSum <- leftSum + dataSeq[mid+1]*log(estimateLeftExpection)
			rightSum <- rightSum - dataSeq[mid+1]*log(estimateRightExpection)
		}	
	}

	probList <- exp(probList - max(probList))
	probList <- probList/sum(probList)

	samplingList <- rmultinom(1,1,probList)
	for (mid in 1:lengthSeq) {
		if (samplingList[mid,1] == 1) {
			estimateChangePoint <- mid
			break
		}
	}
	estimateChangePointList[id] <- estimateChangePoint

	# sampling left expectation
	estimateLeftExpection <- rgamma(1,gammaShape+sum(dataSeq[1:estimateChangePoint]),rate=gammaRate+estimateChangePoint)
	estimateLeftExpectionList[id] <- estimateLeftExpection
	# sampling right expectation
	estimateRightExpection <- rgamma(1,gammaShape+sum(dataSeq[estimateChangePoint:lengthSeq]),
									rate=gammaRate+lengthSeq-estimateChangePoint)
	estimateRightExpectionList[id] <- estimateRightExpection

}
