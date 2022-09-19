BEGIN_IMPLEMENT_FN3(INNER(Registers), long, GetDeliveryDays, A, long lClientRef, long lVehicleTypeRef,
					long lVehicleKindRef/* = NULL_REF*/)


	if (lClientRef == NULL_REF && lVehicleTypeRef == NULL_REF && lVehicleKindRef == NULL_REF)
	{
		internal_error();
		return 0;
	}

	DELIVERY_DAYS DeliveryDays =
		{ /*GetWarehouse().GetFilialRef()*/NULL_REF, lClientRef, lVehicleTypeRef, lVehicleKindRef, GetGoodsItemRef(), GetLotRef() };

	long lDeliveryDays = 0;
	if (!CDeliveryDaysMap::Get()->Lookup(DeliveryDays, lDeliveryDays))
	{
		PROXY(Clients) r_Client(dsDB);
		r_Client->SetLine(lClientRef);

		if (r_Client->IsTakingDeliveryTime() && r_Client->GetTypeOutcomeByBBD() !=TO_BY_N0NE)
		{
			PROXY(DeliveryDays) r_DeliveryDays(dsDB);
			lDeliveryDays = r_DeliveryDays->CalcDeliveryDaysEx(
				GetWarehouse().GetLocationRef(),
				r_Client->GetGazetteerRef(),
				lVehicleTypeRef,
				true,
				lVehicleKindRef);

			if (lDeliveryDays < 0)
			{
				PROXY(Gazetteer) r_Gazetteer(dsDB);
				r_Gazetteer->SetLine(GetWarehouse().GetLocationRef());
				CString from = r_Gazetteer->GetFullAddress();
				r_Gazetteer->SetLine(r_Client->GetGazetteerRef());
				CString to = r_Gazetteer->GetFullAddress();

				PROXY(VehicleTypes) r_VehicleType(dsDB);
				r_VehicleType->SetLine(lVehicleTypeRef);
				CString csVehicleType;
				if(r_VehicleType->IsValidLine())
					csVehicleType = ", транспорт: " + r_VehicleType->GetName();
			}

			double dLogisticDaysPercent = 0;
			long lLogisticDays = 0;
			long lTypeOutcomeByBBD = TO_BY_N0NE;
			if(r_Client->GetLogisticDaysByGoodsItem(GetGoodsItemRef(), dLogisticDaysPercent, lLogisticDays))
			{
				if(dLogisticDaysPercent>EPS && lLogisticDays>0)
				{
					lTypeOutcomeByBBD = r_Client->GetTypeOutcomeByBBD();
				}
				else if(lLogisticDays > 0)
				{
					lTypeOutcomeByBBD = TO_BY_LG_DAYS;
				}
				else if (dLogisticDaysPercent > EPS)
				{
					lTypeOutcomeByBBD = TO_BY_PERCENT;
				}
			}
			else
			{
				dLogisticDaysPercent = r_Client->GetLogisticPercentage();
				lLogisticDays = r_Client->GetLogisticDays();
				lTypeOutcomeByBBD = r_Client->GetTypeOutcomeByBBD();
			}

			// #MSN 11.08.2022 получаем бит "не учитывать ОСГ клиента"
			INNER(GoodsItem)* r_GI = &GetGoodsItem();
			r_GI->IsCustomerRSL();
            // #MSN 

			PROXY(DeliveryDaysByGoods) r_DaysByGoods(dsDB);
			long lDaysByGoods = r_DaysByGoods->CalcDays(r_Client->GetLine(), GetGoodsItemRef());
			if (lDaysByGoods > 0)
			{
				lDeliveryDays += lDaysByGoods;
			}
			else if (!r_GI->IsCustomerRSL())  // #MSN 11.08.2022 если в карточке товара не стоит признак "не учитывать ОСГ клиента"
			{
				if (lTypeOutcomeByBBD == TO_BY_LG_DAYS)
				{
					lDeliveryDays += lLogisticDays;
				}
				else if (lTypeOutcomeByBBD == TO_BY_PERCENT)
				{
					if (dLogisticDaysPercent > 0.0)
					{
						long lLifeTime = 0;
						if (GetLotRef() != NULL_REF && GetLot().GetLifeTime() != 0)
						{
							lLifeTime = GetLot().GetLifeTime();
						}
						else
						{
							lLifeTime = GetGoodsItem().GetLifeTime();
						}

						long lDaysByPercent = static_cast<long>(lLifeTime * dLogisticDaysPercent / 100);
						long sSignalPeriod2 = 0;
						if (GetLotRef() != NULL_REF)
							sSignalPeriod2 = GetLot().GetSignalPeriod2();
						if (sSignalPeriod2 == 0)
							sSignalPeriod2 = GetGoodsItem().GetSignalPeriod2();

						lDaysByPercent -= sSignalPeriod2;
						if (lDaysByPercent > 0)
							lDeliveryDays += lDaysByPercent;
					}
				}
			}
		}  // #MSN
		else
			lDeliveryDays = 0;

		CDeliveryDaysMap::Get()->SetAt(DeliveryDays, lDeliveryDays);
	}

	return lDeliveryDays;

END_IMPLEMENT_RFNC